#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <cmath>
#include <linux/input.h>
#include <linux/uinput.h>
#include <vector>
#include <thread>
#include <unordered_map>
#include "spinlock.h"
#include "imgui.h"
#include "TouchHelperA.h"
#include "Utils.h"
#define maxE 5
#define maxF 10
#define UNGRAB 0
#define GRAB 1

namespace Touch {
    static struct {
        input_event downEvent[2]{{{}, EV_KEY, BTN_TOUCH,       1}, {{}, EV_KEY, BTN_TOOL_FINGER, 1}};
        input_event event[512]{0};
    } input;

    static My_Vector2 touch_scale;
    static My_Vector2 screenSize;
    static std::vector<Device> devices;
    static int nowfd;
    static int orientation = 0;
    static bool initialized = false;
    static bool readOnly = false;
    static bool otherTouch = false;
    static std::function<void(std::vector<Device> *)> callback;
    static spinlock lock;
    static TouchSnapshot g_snapshot;
    static TouchSnapshot g_lastQueued;
    static constexpr size_t kEventQueueSize = 128;
    static TouchSnapshot g_eventQueue[kEventQueueSize];
    static size_t g_eventRead = 0;
    static size_t g_eventWrite = 0;
    static unsigned int g_nextSequence = 1;

    // 动态触控障碍物控制数据
    static My_Vector2 g_WindowPos[100];
    static My_Vector2 g_WindowSize[100];
    static int g_WindowCount = 0;

    void SetTouchObstacle(My_Vector2* pos, My_Vector2* size, int count) {
        lock.lock();
        g_WindowCount = (count > 100) ? 100 : count;
        for (int i = 0; i < g_WindowCount; ++i) {
            g_WindowPos[i] = pos[i];
            g_WindowSize[i] = size[i];
        }
        lock.unlock();
    }

    bool CheckInWindow(const My_Vector2& pos) {
        for (int i = 0; i < g_WindowCount; ++i) {
            if (pos.x >= g_WindowPos[i].x && pos.x <= g_WindowPos[i].x + g_WindowSize[i].x &&
                pos.y >= g_WindowPos[i].y && pos.y <= g_WindowPos[i].y + g_WindowSize[i].y) {
                return true;
            }
        }
        return false;
    }

    // Called with lock held. Queue transitions and movement instead of exposing
    // only a single latest state: a quick tap may contain DOWN and UP between
    // two rendered frames, and both events must reach Dear ImGui in order.
    static void QueueSnapshotIfChanged() {
        if (!g_snapshot.valid) return;
        const bool changed = !g_lastQueued.valid ||
            g_snapshot.down != g_lastQueued.down ||
            g_snapshot.pointerId != g_lastQueued.pointerId ||
            g_snapshot.x != g_lastQueued.x ||
            g_snapshot.y != g_lastQueued.y;
        if (!changed) return;

        g_snapshot.sequence = g_nextSequence++;
        g_eventQueue[g_eventWrite] = g_snapshot;
        const size_t next = (g_eventWrite + 1) % kEventQueueSize;
        if (next == g_eventRead) {
            // Prefer the newest coherent gesture state if the consumer stalls.
            g_eventRead = (g_eventRead + 1) % kEventQueueSize;
        }
        g_eventWrite = next;
        g_lastQueued = g_snapshot;
    }

    void Upload() {
        static bool isFirstDown = true;
        int tmpCnt = 0, tmpCnt2 = 0;
        for (auto &device: devices) {
            for (auto &finger: device.Finger) {
                // 只有处于按下状态且未被 ImGui 拦截的手指事件才下发给系统
                if (finger.isDown && !finger.isSwallowed) {
                    if (tmpCnt2++ > 20) {
                        goto finish;
                    }
                    input.event[tmpCnt].type = EV_ABS;
                    input.event[tmpCnt].code = ABS_X;
                    input.event[tmpCnt].value = (int) finger.pos.x;
                    tmpCnt++;

                    input.event[tmpCnt].type = EV_ABS;
                    input.event[tmpCnt].code = ABS_Y;
                    input.event[tmpCnt].value = (int) finger.pos.y;
                    tmpCnt++;

                    input.event[tmpCnt].type = EV_ABS;
                    input.event[tmpCnt].code = ABS_MT_POSITION_X;
                    input.event[tmpCnt].value = (int) finger.pos.x;
                    tmpCnt++;

                    input.event[tmpCnt].type = EV_ABS;
                    input.event[tmpCnt].code = ABS_MT_POSITION_Y;
                    input.event[tmpCnt].value = (int) finger.pos.y;
                    tmpCnt++;

                    input.event[tmpCnt].type = EV_ABS;
                    input.event[tmpCnt].code = ABS_MT_TRACKING_ID;
                    input.event[tmpCnt].value = finger.id;
                    tmpCnt++;

                    input.event[tmpCnt].type = EV_SYN;
                    input.event[tmpCnt].code = SYN_MT_REPORT;
                    input.event[tmpCnt].value = 0;
                    tmpCnt++;
                }
            }
        }
        finish:
        bool is = false;
        if (tmpCnt == 0) {
            input.event[tmpCnt].type = EV_SYN;
            input.event[tmpCnt].code = SYN_MT_REPORT;
            input.event[tmpCnt].value = 0;
            tmpCnt++;
            if (!isFirstDown) {
                isFirstDown = true;
                input.event[tmpCnt].type = EV_KEY;
                input.event[tmpCnt].code = BTN_TOUCH;
                input.event[tmpCnt].value = 0;
                tmpCnt++;
                input.event[tmpCnt].type = EV_KEY;
                input.event[tmpCnt].code = BTN_TOOL_FINGER;
                input.event[tmpCnt].value = 0;
                tmpCnt++;
            }
        } else {
            is = true;
        }
        input.event[tmpCnt].type = EV_SYN;
        input.event[tmpCnt].code = SYN_REPORT;
        input.event[tmpCnt].value = 0;
        tmpCnt++;

        if (is && isFirstDown) {
            isFirstDown = false;
            write(nowfd, &input, sizeof(struct input_event) * (tmpCnt + 2));
        } else {
            write(nowfd, input.event, sizeof(struct input_event) * tmpCnt);
        }
    }

    static void *TypeA(void *arg) {
        int i = (int) (long) arg;
        Device &device = devices[i];

        int latest = 0;
        input_event inputEvent[64]{0};

        while (initialized) {
            auto readSize = (int32_t) read(device.fd, inputEvent, sizeof(inputEvent));
            if (readSize <= 0 || (readSize % sizeof(input_event)) != 0) {
                continue;
            }
            size_t count = size_t(readSize) / sizeof(input_event);

            lock.lock();
            for (size_t j = 0; j < count; j++) {
                input_event &ie = inputEvent[j];
                if (ie.type == EV_ABS) {
                    if (ie.code == ABS_MT_SLOT) {
                        latest = ie.value;
                        continue;
                    }
                    if (ie.code == ABS_MT_TRACKING_ID) {
                        if (ie.value == -1) {
                            device.Finger[latest].isDown = false;
                        } else {
                            device.Finger[latest].id = (i * 2 + 1) * maxF + latest;
                            device.Finger[latest].isDown = true;
                        }
                        continue;
                    }
                    if (ie.code == ABS_MT_POSITION_X) {
                        device.Finger[latest].id = (i * 2 + 1) * maxF + latest;
                        device.Finger[latest].pos.x = (float) ie.value * device.S2TX;
                        continue;
                    }
                    if (ie.code == ABS_MT_POSITION_Y) {
                        device.Finger[latest].id = (i * 2 + 1) * maxF + latest;
                        device.Finger[latest].pos.y = (float) ie.value * device.S2TY;
                        continue;
                    }
                }
                if (ie.code == SYN_REPORT) {
                    // 状态锁存核心逻辑：判断每根手指落下的瞬间是否点击在窗口内
                    for (int f = 0; f < 10; ++f) {
                        touchObj &finger = device.Finger[f];
                        if (finger.isDown) {
                            if (!finger.wasDown) { 
                                My_Vector2 screen_pos = Touch2Screen(finger.pos);
                                finger.isSwallowed = CheckInWindow(screen_pos);
                                finger.wasDown = true;
                            }
                        } else {
                            finger.wasDown = false;
                            finger.isSwallowed = false;
                        }
                    }

                    // Match the template's window-latched touch behavior while
                    // keeping all Dear ImGui calls on the render thread. A
                    // pointer is eligible only if its initial DOWN landed in a
                    // registered UI obstacle (isSwallowed). Once selected, keep
                    // the same pointer until UP so multi-touch cannot make the
                    // cursor jump between fingers.
                    touchObj *primary = nullptr;
                    const bool hadPrimary = g_snapshot.down && g_snapshot.pointerId >= 0;
                    if (hadPrimary) {
                        for (auto &d : devices) {
                            for (int f = 0; f < 10; ++f) {
                                touchObj &finger = d.Finger[f];
                                if (finger.id == g_snapshot.pointerId &&
                                    finger.isDown && finger.isSwallowed) {
                                    primary = &finger;
                                    break;
                                }
                            }
                            if (primary) break;
                        }
                    }
                    if (!primary && !hadPrimary) {
                        for (auto &d : devices) {
                            for (int f = 0; f < 10; ++f) {
                                touchObj &finger = d.Finger[f];
                                if (finger.isDown && finger.isSwallowed) {
                                    primary = &finger;
                                    break;
                                }
                            }
                            if (primary) break;
                        }
                    }

                    if (primary) {
                        My_Vector2 screenPos = Touch2Screen(primary->pos);
                        g_snapshot.x = screenPos.x;
                        g_snapshot.y = screenPos.y;
                        g_snapshot.down = true;
                        g_snapshot.valid = true;
                        g_snapshot.pointerId = primary->id;
                    } else if (g_snapshot.down) {
                        // Preserve the final position for one release event.
                        // ImGui resolves button clicks on UP at the last pointer
                        // position; invalidating the coordinates first would
                        // incorrectly cancel the click.
                        g_snapshot.down = false;
                        g_snapshot.valid = true;
                        g_snapshot.pointerId = -1;
                    } else {
                        // Keep the most recent release state valid. The event
                        // queue emits UP exactly once and later idle reports do
                        // not enqueue duplicates.
                        g_snapshot.down = false;
                        g_snapshot.pointerId = -1;
                    }
                    QueueSnapshotIfChanged();

                    if (!readOnly) {
                        if (callback) {
                            callback(&devices);
                        } else {
                            Upload();
                        }
                    }
                    continue;
                }
            }
            lock.unlock();
        }
        return nullptr;
    }

    static bool checkDeviceIsTouch(int fd) {
        uint8_t *bits = NULL;
        ssize_t bits_size = 0;
        int res, j, k;
        bool itmp = false, itmp2 = false, itmp3 = false;
        struct input_absinfo abs{};
        while (true) {
            res = ioctl(fd, EVIOCGBIT(EV_ABS, bits_size), bits);
            if (res < bits_size)
                break;
            bits_size = res + 16;
            bits = (uint8_t *) realloc(bits, bits_size * 2);
        }
        for (j = 0; j < res; j++) {
            for (k = 0; k < 8; k++)
                if (bits[j] & 1 << k && ioctl(fd, EVIOCGABS(j * 8 + k), &abs) == 0) {
                    if (j * 8 + k == ABS_MT_SLOT) {
                        itmp = true;
                        continue;
                    }
                    if (j * 8 + k == ABS_MT_POSITION_X) {
                        itmp2 = true;
                        continue;
                    }
                    if (j * 8 + k == ABS_MT_POSITION_Y) {
                        itmp3 = true;
                        continue;
                    }
                }
        }
        free(bits);
        return itmp && itmp2 && itmp3;
    }

    bool Init(const My_Vector2 &s, bool p_readOnly) {
        Close();
        devices.clear();
        g_snapshot = TouchSnapshot{};
        g_lastQueued = TouchSnapshot{};
        g_eventRead = g_eventWrite = 0;
        g_nextSequence = 1;
        My_Vector2 size = s;
        readOnly = p_readOnly;
        if (size.x > size.y) {
            screenSize = size;
        } else {
            screenSize = {size.y, size.x};
        }
        DIR *dir = opendir("/dev/input/");
        if (!dir) {
            return false;
        }

        // Enumerate the actual event node names. The template counted entries
        // and then assumed event0..eventN were contiguous, which misses touch
        // devices on systems with gaps in their event numbering.
        dirent *ptr = nullptr;
        while ((ptr = readdir(dir)) != nullptr) {
            if (strncmp(ptr->d_name, "event", 5) != 0) continue;
            char path[160];
            snprintf(path, sizeof(path), "/dev/input/%s", ptr->d_name);
            const int flags = (readOnly ? O_RDONLY : O_RDWR) | O_CLOEXEC;
            int fd = open(path, flags);
            if (fd < 0) continue;

            if (checkDeviceIsTouch(fd)) {
                Device device{};
                if (ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &device.absX) == 0
                    && ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), &device.absY) == 0) {
                    device.fd = fd;
                    if (!readOnly) ioctl(fd, EVIOCGRAB, GRAB);
                    devices.push_back(device);
                    continue;
                }
            }
            close(fd);
        }
        closedir(dir);

        if (devices.empty()) {
            puts("获取屏幕驱动失败");
            return false;
        }

        int screenX = devices[0].absX.maximum;
        int screenY = devices[0].absY.maximum;

        if (!readOnly) {
            struct uinput_user_dev ui_dev;
            nowfd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
            if (nowfd <= 0) {
                return false;
            }

            int string_len = rand() % 10 + 5;
            char string[16]{};
            memset(&ui_dev, 0, sizeof(ui_dev));

            genRandomString(string, string_len);
            strncpy(ui_dev.name, string, UINPUT_MAX_NAME_SIZE);

            ui_dev.id.bustype = 0;
            ui_dev.id.vendor = rand() % 10 + 5;
            ui_dev.id.product = rand() % 10 + 5;
            ui_dev.id.version = rand() % 10 + 5;

            ioctl(nowfd, UI_SET_PROPBIT, INPUT_PROP_DIRECT);

            ioctl(nowfd, UI_SET_EVBIT, EV_ABS);
            ioctl(nowfd, UI_SET_ABSBIT, ABS_X);
            ioctl(nowfd, UI_SET_ABSBIT, ABS_Y);
            ioctl(nowfd, UI_SET_ABSBIT, ABS_MT_POSITION_X);
            ioctl(nowfd, UI_SET_ABSBIT, ABS_MT_POSITION_Y);
            ioctl(nowfd, UI_SET_ABSBIT, ABS_MT_TRACKING_ID);
            ioctl(nowfd, UI_SET_EVBIT, EV_SYN);
            ioctl(nowfd, UI_SET_EVBIT, EV_KEY);
            ioctl(nowfd, UI_SET_KEYBIT, BTN_TOOL_FINGER);
            ioctl(nowfd, UI_SET_KEYBIT, BTN_TOUCH);

            genRandomString(string, string_len);
            ioctl(nowfd, UI_SET_PHYS, string);

            int fd = devices[0].fd;
            {
                struct input_id id{};
                if (ioctl(fd, EVIOCGID, &id) == 0) {
                    ui_dev.id.bustype = id.bustype;
                    ui_dev.id.vendor = id.vendor;
                    ui_dev.id.product = id.product;
                    ui_dev.id.version = id.version;
                }
                uint8_t *bits = NULL;
                ssize_t bits_size = 0;
                int res, j, k;
                while (1) {
                    res = ioctl(fd, EVIOCGBIT(EV_KEY, bits_size), bits);
                    if (res < bits_size)
                        break;
                    bits_size = res + 16;
                    bits = (uint8_t *) realloc(bits, bits_size * 2);
                }
                for (j = 0; j < res; j++) {
                    for (k = 0; k < 8; k++)
                        if (bits[j] & 1 << k) {
                            if (j * 8 + k == BTN_TOUCH || j * 8 + k == BTN_TOOL_FINGER)
                                continue;
                            ioctl(nowfd, UI_SET_KEYBIT, j * 8 + k);
                        }
                }
                free(bits);
            }
            ui_dev.absmin[ABS_MT_POSITION_X] = 0;
            ui_dev.absmax[ABS_MT_POSITION_X] = screenX;
            ui_dev.absmin[ABS_MT_POSITION_Y] = 0;
            ui_dev.absmax[ABS_MT_POSITION_Y] = screenY;
            ui_dev.absmin[ABS_X] = 0;
            ui_dev.absmax[ABS_X] = screenX;
            ui_dev.absmin[ABS_Y] = 0;
            ui_dev.absmax[ABS_Y] = screenY;
            ui_dev.absmin[ABS_MT_TRACKING_ID] = 0;
            ui_dev.absmax[ABS_MT_TRACKING_ID] = 65535;
            write(nowfd, &ui_dev, sizeof(ui_dev));

            if (ioctl(nowfd, UI_DEV_CREATE)) {
                return false;
            }
        }
        initialized = true;

        pthread_t t;
        for (int i = 0; i < devices.size(); i++) {
            devices[i].S2TX = (float) screenX / (float) devices[i].absX.maximum;
            devices[i].S2TY = (float) screenY / (float) devices[i].absY.maximum;
            pthread_create(&t, nullptr, TypeA, (void *) (long) i);
        }
        if (size.x > size.y) {
            std::swap(size.x, size.y);
        }
        if (otherTouch) {
            std::swap(size.x, size.y);
        }
        touch_scale.x = (float) screenX / size.x;
        touch_scale.y = (float) screenY / size.y;

        return true;
    }

    bool GetSnapshot(TouchSnapshot *out) {
        if (!out) return false;
        lock.lock();
        if (g_eventRead == g_eventWrite) {
            lock.unlock();
            return false;
        }
        *out = g_eventQueue[g_eventRead];
        g_eventRead = (g_eventRead + 1) % kEventQueueSize;
        lock.unlock();
        return true;
    }

    void Close() {
        if (initialized) {
            for (auto &device: devices) {
                if (!readOnly)
                    ioctl(device.fd, EVIOCGRAB, UNGRAB);
                close(device.fd);
                device.fd = 0;
            }
            if (nowfd > 0) {
                ioctl(nowfd, UI_DEV_DESTROY);
                close(nowfd);
                nowfd = 0;
            }
            memset(input.event, 0, sizeof(input.event));
            initialized = false;
            devices.clear();
        }
    }

    void Down(float x, float y) {
        lock.lock();
        touchObj &touch = devices[0].Finger[9];
        touch.id = 19;
        touch.pos = My_Vector2(x, y) * touch_scale;
        touch.isDown = true;
        Upload();
        lock.unlock();
    }

    void Move(touchObj *touch, float x, float y) {
        lock.lock();
        touch->pos = My_Vector2(x, y) * touch_scale;
        Upload();
        lock.unlock();
    }

    void Move(float x, float y) {
        Down(x, y);
    }

    void Up() {
        lock.lock();
        touchObj &touch = devices[0].Finger[9];
        touch.isDown = false;
        Upload();
        lock.unlock();
    }

    void SetCallBack(const std::function<void(std::vector<Device> *)> &cb) {
        callback = cb;
    }

    My_Vector2 Touch2Screen(const My_Vector2 &coord) {
        float x = coord.x, y = coord.y;
        float xt = x / touch_scale.x;
        float yt = y / touch_scale.y;

        if (otherTouch) {
            switch (orientation) {
                case 1:
                    x = xt;
                    y = yt;
                    break;
                case 2:
                    y = yt;
                    x = screenSize.y - xt;
                    break;
                case 3:
                    x = screenSize.y - xt;
                    y = screenSize.x - yt;
                    break;
                default:
                    y = xt;
                    x = screenSize.y - yt;
                    break;
            }
        } else {
            switch (orientation) {
                case 1:
                    x = yt;
                    y = screenSize.y - xt;
                    break;
                case 2:
                    x = screenSize.y - xt;
                    y = screenSize.x - yt;
                    break;
                case 3:
                    y = xt;
                    x = screenSize.x - yt;
                    break;
                default:
                    x = xt;
                    y = yt;
                    break;
            }
        }
        return {x, y};
    }

    My_Vector2 GetScale() {
        return touch_scale;
    }

    void setOrientation(int o) {
        orientation = o;
    }

    void setOtherTouch(bool p_otherTouch) {
        otherTouch = p_otherTouch;
    }
}
