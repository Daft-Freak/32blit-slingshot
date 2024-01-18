#include "32blit.hpp"
#include "assets.hpp"

using namespace blit;

static std::string path = "/";

// TODO: custom struct if we need more info
static std::vector<FileInfo> file_list;
static int file_list_offset = 0;

static Point scroll_offset;

static std::string join_path(const std::string &a, const std::string &b) {
    std::string ret;
    ret.reserve(a.length() + b.length() + 1);

    ret = a;

    if(ret[ret.length() - 1] != '/')
        ret += '/';
    
    ret += b;

    return ret;
}

static bool should_display_file(const std::string &path) {
    // TODO: can_launch api
    auto last_dot = path.find_last_of('.');

    auto ext = last_dot == std::string::npos ? "" : path.substr(last_dot + 1);

    for(auto &c : ext)
        c = tolower(c);

    return ext == "blit";
}

static void update_file_list() {
    file_list = list_files(path, [](const FileInfo &info){
        // hidden file
        if(info.name[0] == '.')
            return false;

        if(info.flags & FileFlags::directory)
            return true;

        return should_display_file(join_path(path, info.name));
    });

    file_list_offset = 0;
    scroll_offset.x = 0;
}

void init() {
    // TODO: list_installed_games, top level installed/filesystem
    // TODO: remember last path
    update_file_list();
}

void render(uint32_t time) {

    screen.pen = Pen(0, 0, 0);
    screen.clear();

    // draw some text at the top of the screen
    screen.pen = Pen(255, 255, 255);
    screen.rectangle(Rect(0, 0, 320, 12));

    // TODO: scroll if too long
    screen.pen = Pen(0, 0, 0);
    screen.text(path, minimal_font, Point(5, 3));

    const Size splash_size(128, 96);

    Point center_pos(screen.bounds.w / 2, screen.bounds.h / 2);
    int full_list_width = file_list.size() * screen.bounds.w;

    int i = 0;
    for(auto &info : file_list) {
        auto file_pos = Point(i * screen.bounds.w, 0);

        auto offset = file_pos - scroll_offset;

        // wrap
        if(offset.x > screen.bounds.w && scroll_offset.x < 0)
            offset.x -= full_list_width;
        else if(offset.x <= -screen.bounds.w)
            offset.x += full_list_width;

        if(offset.x > screen.bounds.w || offset.x < -screen.bounds.w) {
            // skip completely offscreen
            i++;
            continue;
        }

        auto splash_center = offset + center_pos;

        // splash placeholder
        screen.pen = {127, 0, 255};
        screen.rectangle({splash_center - Point(splash_size.w, splash_size.h) / 2, splash_size});

        screen.pen = {255, 255, 255};
        screen.text(info.name, minimal_font, splash_center + Point(0, splash_size.h / 2 + 6), true, TextAlign::center_center);

        i++;
    }

    // old item scrolling out
    // TODO: store info
    if(scroll_offset.y != 0) {
        Point offset;
        offset.x = -(scroll_offset.x % screen.bounds.w);
        offset.y = scroll_offset.y < 0 ? -screen.bounds.h - scroll_offset.y : screen.bounds.h - scroll_offset.y ;

        auto splash_center = offset + center_pos;

        // splash placeholder
        screen.pen = {127, 0, 255};
        screen.rectangle({splash_center - Point(splash_size.w, splash_size.h) / 2, splash_size});
    }
}

void update(uint32_t time) {

    // update scroll
    int scroll_target_x = file_list_offset * screen.bounds.w;
    if(scroll_offset.x != scroll_target_x) {
        int dir = scroll_offset.x < scroll_target_x ? 1 : -1;

        scroll_offset.x += dir * 5;
    }

    if(scroll_offset.y != 0) {
        int dir = scroll_offset.y < 0 ? 1 : -1;

        scroll_offset.y += dir * 3;
    }

    if(!file_list.empty()) {
        // list scrolling
        if(buttons.released & Button::DPAD_LEFT) {
            file_list_offset--;
            if(file_list_offset < 0) {
                file_list_offset += file_list.size();
                // wrap scroll pos as well
                scroll_offset.x += file_list.size() * screen.bounds.w;
            }
        
        } else if(buttons.released & Button::DPAD_RIGHT) {
            file_list_offset++;
            if(file_list_offset >= int(file_list.size())) {
                file_list_offset = 0;
                scroll_offset.x -= file_list.size() * screen.bounds.w;
            }
        }

        if(buttons.released & Button::A) {
            auto &current_file = file_list[file_list_offset];

            if(current_file.flags & FileFlags::directory) {
                path = join_path(path, current_file.name);
                update_file_list();

                scroll_offset.y -= screen.bounds.h;
            } else {
                // launch, probably
            }
        }

        if(buttons.released & Button::B) {
            if(path != "/") {
                // go up
                auto pos = path.find_last_of('/', path.length() - 2);
                if(pos == 0)
                    path = "/";
                else
                    path = path.substr(0, pos);

                update_file_list();
                scroll_offset.y += screen.bounds.h;
            }
        }
    }
}