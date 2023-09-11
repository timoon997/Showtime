#include "inotify_manager.h"

#include <iostream>
#include <filesystem>
#include <exception>

#include <poll.h>
#include <unistd.h>

namespace inotify_manager {

uint32_t const WD_MASK = IN_CREATE | IN_DELETE | IN_MODIFY;

InotifyManager::InotifyManager(std::string const general_path)
    : _general_path(general_path)
{
    auto ret = this->init_inotify();
    if (ret)
    {
        throw std::runtime_error("Can't init inotify");
    }
}

InotifyManager::~InotifyManager()
{
    this->fini_inotify();
}

void InotifyManager::fini_inotify()
{
    for (const auto& [fd, path] : _work_dirs)
    {
        // Skip the errors, just try to remove everything...
        inotify_rm_watch(this->_file_descriptor, fd);
    }

    auto ret = close(this->_file_descriptor);
    if (ret)
    {
        std::cout << "close() failed, can't close inotify fd" << std::endl;
    }
}

int InotifyManager::init_inotify()
{
    this->_file_descriptor = inotify_init();
    if (this->_file_descriptor < 0)
    {
        std::cout << "inotify_init() failed, invalid file descriptor" << std::endl;
        return -1;
    }

    auto watch_descr = inotify_add_watch(this->_file_descriptor, _general_path.data(), WD_MASK);
    if (-1 == watch_descr)
    {
        std::cout << "inotify_add_watch() failed, can't add " <<
            _general_path << std::endl;
        return -2;
    }

    // Add watching folders to the storage
    this->_work_dirs.insert(std::make_pair(watch_descr, _general_path));

    for (auto const &dir_entry : std::filesystem::recursive_directory_iterator(_general_path))
    {
        if (std::filesystem::is_directory(dir_entry.status()))
        {
            watch_descr = inotify_add_watch(this->_file_descriptor,
                dir_entry.path().c_str(), WD_MASK);
            if (-1 == watch_descr)
            {
                std::cout << "inotify_add_watch() failed, can't add " <<
                    dir_entry.path() << std::endl;
                return -3;
            }

            this->_work_dirs.insert(std::make_pair(watch_descr, dir_entry.path()));
        }
    }

    return 0;
}

int InotifyManager::handle_event()
{
    constexpr int BUF_LEN = sizeof(struct inotify_event) + FILENAME_MAX;
    char buffer[BUF_LEN] = {};
    ssize_t const len = read(this->_file_descriptor, buffer, BUF_LEN);
    struct inotify_event const *const event = (struct inotify_event *)buffer;

    if (-1 == len)
    {
        std::cout << "Failed to read event" << std::endl;
        return -1;
    }

    // Skip empty event.
    if (!event->len)
    {
        return 0;
    }

    std::string const full_path = this->_work_dirs[event->wd] + "/" + event->name;

    if (event->mask & IN_CREATE)
    {
        if (event->mask & IN_ISDIR)
        {
            std::cout << "Folder \""<< full_path << "\" has been created" << std::endl;

            auto watch_descr = inotify_add_watch( this->_file_descriptor, full_path.data(), WD_MASK);
            if (watch_descr < 0)
            {
                std::cout << "inotify_add_watch() failed, can't add " << full_path << std::endl;
                return -2;
            }

            this->_work_dirs.insert(std::make_pair(watch_descr, full_path));
        }
        else
        {
            std::cout << "File \""<< full_path << "\" has been created" << std::endl;
        }
    }
    else if (event->mask & IN_DELETE)
    {
        // TODO: for folders that were marked as watched by inotify
        // we don't get events about their removing. I didn't have enought time
        // to investigate it, but for simple files everything is ok.
        if (event->mask & IN_ISDIR)
        {
            for (const auto& [fd, path] : _work_dirs)
            {
                if (full_path == path)
                {
                    this->_work_dirs.erase(fd);
                    std::cout << "Folder \""<< full_path << "\" has been removed" << std::endl;
                }
            }
        }
        else
        {
            std::cout << "File \""<< full_path << "\" has been removed" << std::endl;
        }
    }
    else if (event->mask & IN_MODIFY)
    {
        std::cout << (event->mask & IN_ISDIR ? "Folder" : "File")
            << full_path << "\" has been modified" << std::endl;
    }

    return 0;
}

int InotifyManager::run()
{
    struct pollfd fds;
    constexpr nfds_t POLL_DESCRIPTOR_COUNT = 1;
    constexpr int INOTIFY_POLL_TIMEOUT = 1000;

    fds.fd = this->_file_descriptor;
    fds.events = POLLIN;

    this->_inotify_manager_is_init = true;

    while (this->_inotify_manager_is_init)
    {
        auto fd_count = poll(&fds, POLL_DESCRIPTOR_COUNT, INOTIFY_POLL_TIMEOUT);

        if (-1 == fd_count)
        {
            return -1;
        }

        if (fds.revents & POLLIN)
        {
            // I am not sure about count of fd in the one iteration, and
            // if it is more than 1, WARN is going to show us about this error
            if (fd_count > 1)
            {
                std::cout << "WARN: fd_count more that 1" << fd_count << std::endl;
            }

            auto ret = this->handle_event();
            if (ret)
            {
                std::cout << "Something went wrong" << std::endl;
                return -2;
            }
        }
    }

    return 0;
}

void InotifyManager::stop()
{
    this->_inotify_manager_is_init = false;
}

}
