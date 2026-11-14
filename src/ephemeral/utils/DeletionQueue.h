#pragma once
/**
 * @file Camera.h
 * @author Alan Abraham P Kochumon
 * @date Created on: November 13, 2026
 *
 * @brief A simple data structure for deleting objects in a LIFO manner.
 *
 * @copyright Copyright (c) 2026 Alan Abraham P Kochumon
 */

#include <deque>
#include <functional>

namespace ephemeral
{

    struct DeletionQueue
    {
        std::deque<std::function<void()>> queue;

        /**
         * @brief Add a function to the back of the deletion queue.
         */
        void push(std::function<void()>&& function) { queue.push_back(function); }

        /**
         * @brief Delete all the objects in a Last In First Out manner.
         */
        void flush()
        {
            for (auto it = queue.rbegin(); it != queue.rend(); ++it)
            {
                (*it)();
            }
            queue.clear();
        }
    };
} // namespace ephemeral
