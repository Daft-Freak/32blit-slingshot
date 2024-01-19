#include <cstring>
#include <list>
#include <optional>

#include "32blit.hpp"
#include "metadata.hpp"

#include "assets.hpp"

using namespace blit;

struct MetadataCacheEntry {
    BlitGameMetadata data;
    bool valid = false;
    std::string path;
};

std::list<MetadataCacheEntry> metadata_cache;

static std::string path = "/";

// TODO: custom struct if we need more info
static std::vector<FileInfo> file_list;
static int file_list_offset = 0;

static Point scroll_offset;

static Surface *default_splash, *folder_splash;

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

// TODO: this is copied from the SDK launcher...
static bool parse_file_metadata(const std::string &filename, BlitGameMetadata &metadata, bool unpack_images = false) {
    blit::File f(filename);

    if(!f.is_open())
        return false;

    uint32_t offset = 0;

    uint8_t buf[sizeof(BlitGameHeader)];
    auto read = f.read(offset, sizeof(buf), (char *)&buf);

    // skip relocation data
    if(memcmp(buf, "RELO", 4) == 0) {
        uint32_t num_relocs;
        f.read(4, 4, (char *)&num_relocs);

        offset = num_relocs * 4 + 8;
        // re-read header
        read = f.read(offset, sizeof(buf), (char *)&buf);
    }

    // game header - skip to metadata
    if(memcmp(buf, "BLITMETA", 8) != 0) {
        auto &header = *(BlitGameHeader *)buf;
        if(read == sizeof(BlitGameHeader) && header.magic == blit_game_magic) {
            metadata.device_id = header.device_id;
            metadata.api_version_major = header.device_id == BlitDevice::STM32BlitOld ? 0 : header.api_version_major;
            metadata.api_version_minor = header.device_id == BlitDevice::STM32BlitOld ? 0 : header.api_version_minor;

            offset += (header.end & 0x1FFFFFF);
            read = f.read(offset, 10, (char *)buf);
        }
    }

    if(read >= 10 && memcmp(buf, "BLITMETA", 8) == 0) {
        // don't bother reading the whole thing if we don't want the images
        auto metadata_len = unpack_images ? *reinterpret_cast<uint16_t *>(buf + 8) : sizeof(RawMetadata);

        uint8_t metadata_buf[0xFFFF];
        f.read(offset + 10, metadata_len, (char *)metadata_buf);

        parse_metadata(reinterpret_cast<char *>(metadata_buf), metadata_len, metadata, unpack_images);

        return true;
    }

    return false;
}

static BlitGameMetadata *get_metadata(const std::string &path) {

    for(auto it = metadata_cache.begin(); it != metadata_cache.end(); ++it) {
        if(it->path == path) {
            // move to top
            metadata_cache.splice(metadata_cache.begin(), metadata_cache, it);

            if(it->valid)
                return &it->data;
            
            return nullptr; // cached failure
        }
    }

    // reuse least recently used
    auto it = std::prev(metadata_cache.end());

    it->valid = parse_file_metadata(path, it->data, true);
    it->path = path;

    metadata_cache.splice(metadata_cache.begin(), metadata_cache, it);

    return it->valid ? &it->data : nullptr;
}

void init() {
    default_splash = Surface::load(asset_no_image);
    folder_splash = Surface::load(asset_folder_splash);

    metadata_cache.emplace_front(MetadataCacheEntry{});
    metadata_cache.emplace_front(MetadataCacheEntry{});

    // TODO: list_installed_games, top level installed/filesystem
    // TODO: remember last path
    update_file_list();
}

void render(uint32_t time) {

    screen.pen = Pen(0, 0, 0);
    screen.clear();

    const Size splash_size(128, 96);
    const Point splash_half_size(splash_size.w / 2, splash_size.h / 2);

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

        // fetch metadata
        bool is_dir = info.flags & FileFlags::directory;

        auto metadata = is_dir ? nullptr : get_metadata(join_path(path, info.name));

        // splash
        auto splash_center = offset + center_pos;
        auto splash_image = is_dir ? folder_splash : default_splash;

        if(metadata)
            splash_image = metadata->splash;

        float scale = 1.0f - Vec2(offset).length() / screen.bounds.w;
        screen.stretch_blit(splash_image, {Point(0, 0), splash_size}, {splash_center - splash_half_size * scale, splash_center + splash_half_size * scale});

        // game title/dir name
        screen.pen = {255, 255, 255};
        auto label = metadata ? metadata->title : info.name;
        screen.text(label, minimal_font, splash_center + Point(0, (splash_size.h / 2) * scale + 6), true, TextAlign::center_center);

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
        float scale = 1.0f - Vec2(offset).length() / screen.bounds.w;
        screen.stretch_blit(folder_splash, {Point(0, 0), splash_size}, {splash_center - splash_half_size * scale, splash_center + splash_half_size * scale});
    }

    // draw current path
    screen.pen = Pen(255, 255, 255);
    screen.rectangle(Rect(0, 0, 320, 12));

    // TODO: scroll if too long
    screen.pen = Pen(0, 0, 0);
    screen.text(path, minimal_font, Point(5, 3));
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