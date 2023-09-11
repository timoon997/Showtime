#pragma once

#include <map>
#include <atomic>
#include <string>
#include <sys/inotify.h>

namespace inotify_manager {

class InotifyManager
{
public:
    InotifyManager(std::string path);

    ~InotifyManager();

    int run();

    void stop();

private:
    int init_inotify();

    void fini_inotify();

    int handle_event();

    std::atomic<bool> _inotify_manager_is_init {};
    // Storage for watching folders
    std::map<int, std::string> _work_dirs {};
    std::string _general_path {};
    int _file_descriptor {};
};

}
