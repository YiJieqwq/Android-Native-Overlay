# Touch input architecture

## Design goals

- Preserve the template's "touch begins inside a registered ImGui window" latch.
- Keep Dear ImGui calls on the render thread; input-reader threads only publish state.
- Never use `EVIOCGRAB` or uinput in the UI-only build (`Touch::Init(..., true)`).
- Keep one primary pointer from DOWN through UP so a second finger cannot move the cursor.
- Preserve the final coordinates for the UP event because ImGui resolves clicks on release.

## Flow

```text
/dev/input/event*
  -> Type-A/slot parser
  -> Touch2Screen orientation transform
  -> initial-DOWN obstacle test
  -> primary pointer event queue + sequence
  -> render-thread drawBegin()
  -> ImGui queued touch events
```

The obstacle is expressed in physical display coordinates. `drawBegin()` converts the selected physical pointer to the bounded Surface's local coordinates before calling `AddMousePosEvent()`.

## Differences from the reference template

The reference template writes `io.MousePos` and `io.MouseDown` directly from the input thread. This project retains its useful obstacle-latching behavior but uses a locked event queue instead, avoiding concurrent access to Dear ImGui state and preserving a fast DOWN/UP pair that may occur between two rendered frames.

## Regression checklist on device

Test both portrait and landscape:

1. Tap every button once; confirm one action per release.
2. Press inside a button, move outside, release; confirm expected ImGui cancellation behavior.
3. Drag the title handle beyond the old panel edge; confirm drag remains latched until UP.
4. Scroll the log with a finger and release outside the log child.
5. Add a second finger during a drag; confirm the primary pointer does not jump.
6. Touch outside the panel; confirm the UI receives no click.
7. Collapse the panel and interact with the underlying app throughout the former lower 1000×1050 area; confirm taps and swipes are delivered. Confirm the terminal reports `surface rebound=1000x150 actual=1000x150`, the traffic lights/text/expand button remain visible, and expansion returns to `1000x1200` without losing textures or state.
8. Rotate the display; allow 250 ms for orientation polling and repeat tests.
9. Confirm underlying apps remain usable because observer mode does not grab input.
