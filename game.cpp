#include "32blit.hpp"

using namespace blit;

void init() {
}

void render(uint32_t time) {

    screen.pen = Pen(0, 0, 0);
    screen.clear();

    // draw some text at the top of the screen
    screen.pen = Pen(255, 255, 255);
    screen.rectangle(Rect(0, 0, 320, 14));

    screen.pen = Pen(0, 0, 0);
    screen.text("Hello 32blit!", minimal_font, Point(5, 4));
}

void update(uint32_t time) {
}