/** Just simple manager for monitoring and logging actions for some folder @file.
 *
 * Set path and run this progamm.
 * You are going to see log-messages about changes in the watching folder.
 *
 * You can interrupt this process by SIGINT (сtrl+c UNIX)
 *
 */

#include "inotify_manager.h"

#include <iostream>
#include <memory>
#include <exception>
#include <functional>
#include <csignal>

std::function<void()> g_interrupt_handler;

// Interrupt work of this process
void sigint_handler(int)
{
    if (g_interrupt_handler)
    {
        g_interrupt_handler();
    }
}

int main()
{
    using namespace inotify_manager;

    std::signal(SIGINT, sigint_handler);
    std::string const path = "/home/timon/inotify";

    std::unique_ptr<InotifyManager> inotify_manager;

    try
    {
        inotify_manager = std::make_unique<InotifyManager>(path);
    }
    catch (std::runtime_error &ex)
    {
        std::cout << ex.what() << std::endl;
        return -1;
    }
    catch(...)
    {
        std::cout << "Catch some exception" << std::endl;
        return -2;
    }

    try
    {
        // Do not check return value, for this tested manager it doesn't have sense.
        inotify_manager->run();

        g_interrupt_handler = [&inotify_manager]()
        {
            inotify_manager->stop();
        };
    }
    catch(...)
    {
        std::cout << "Catch some exception" << std::endl;
        return -3;
    }

    return 0;
}
