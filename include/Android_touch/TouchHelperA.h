#pragma once

#include <linux/input.h>
#include <vector>
#include <functional>
#include "VectorStruct.h"

namespace Touch {
    struct touchObj {
        My_Vector2 pos{};
        int id = 0;
        bool isDown = false;
        bool wasDown = false;       // 追踪状态过渡
        bool isSwallowed = false;   // 标记该手指是否被窗口拦截
    };

    struct Device {
        int fd;
        float S2TX;
        float S2TY;
        input_absinfo absX, absY;
        touchObj Finger[10];

        Device() { memset((void *) this, 0, sizeof(*this)); }
    };

    struct TouchSnapshot {
        float x = -1.0f;
        float y = -1.0f;
        bool down = false;
        bool valid = false;
        int pointerId = -1;
        unsigned int sequence = 0;
    };

    bool Init(const My_Vector2 &s, bool p_readOnly);

    // Copies the latest input state without calling Dear ImGui from input threads.
    bool GetSnapshot(TouchSnapshot *out);

    void Close();

    void Down(float x, float y);

    void Move(float x, float y);

    void Up();

    void Move(touchObj *touch, float x, float y);

    void Upload();

    // 设置窗口障碍坐标和大小
    void SetTouchObstacle(My_Vector2* pos, My_Vector2* size, int count);
    
    // 检测坐标是否落入任何一个窗口内部
    bool CheckInWindow(const My_Vector2& pos);

    void SetCallBack(const std::function<void(std::vector<Device> *)> &cb);

    My_Vector2 Touch2Screen(const My_Vector2 &coord);

    My_Vector2 GetScale();

    void setOrientation(int orientation);

    void setOtherTouch(bool p_otherTouch);
}
