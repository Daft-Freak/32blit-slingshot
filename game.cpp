#include "32blit.hpp"
#include "assets.hpp"

using namespace blit;

static std::string path = "/";

// TODO: custom struct if we need more info
static std::vector<FileInfo> file_list;
static int file_list_offset = 0;

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
    screen.rectangle(Rect(0, 0, 320, 14));

    // TODO: scroll if too long
    screen.pen = Pen(0, 0, 0);
    screen.text(path, minimal_font, Point(5, 4));

    if(!file_list.empty()) {
        screen.pen = {255, 255, 255};
        screen.text(file_list[file_list_offset].name, minimal_font, blit::Point(screen.bounds.w, screen.bounds.h) / 2, true, TextAlign::center_center);
    }
}

void update(uint32_t time) {

    if(!file_list.empty()) {
        // list scrolling
        if(buttons.released & Button::DPAD_LEFT) {
            file_list_offset--;
            if(file_list_offset < 0)
                file_list_offset += file_list.size();
        } else if(buttons.released & Button::DPAD_RIGHT) {
            file_list_offset++;
            if(file_list_offset >= int(file_list.size()))
                file_list_offset = 0;
        }

        if(buttons.released & Button::A) {
            auto &current_file = file_list[file_list_offset];

            if(current_file.flags & FileFlags::directory) {
                path = join_path(path, current_file.name);
                update_file_list();
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
            }
        }
    }
}