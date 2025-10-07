## OneShot.hpp

Single-use one-shot future

### Timed get and broken promise

```
#include "one_shot.hpp"
#include <iostream>
#include <thread>

int main() {
    auto [sender, receiver] = OneShot<int>::make();

    std::thread t([s = std::move(sender)]() mutable {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        s.set_value(42);
    });

    if (auto val = receiver.get_for(std::chrono::milliseconds(100))) {
        std::cout << "Got: " << *val << "\n";
    } else {
        std::cout << "Timed out... waiting fully\n";
        std::cout << "Got later: " << receiver.get() << "\n";
    }

    t.join();
}
```

### void variant and broken sender

```
#include "one_shot.hpp"
#include <iostream>

int main() {
    auto [sender, receiver] = OneShot<void>::make();

    // destroy sender without setting value
    sender = {};

    try {
        receiver.get();
    } catch (const std::future_error& e) {
        std::cout << "Caught: " << e.what() << "\n";
    }
}
```

## OneShotChannel.hpp

Reusable

### Reusable Channel

```
#include "one_shot_channel.hpp"
#include <iostream>
#include <thread>

int main() {
    auto [sender, receiver] = OneShotChannel<int>::make();

    for (int i = 0; i < 3; ++i) {
        std::thread worker([s = sender, i]() mutable {
            std::this_thread::sleep_for(std::chrono::milliseconds(100 + i * 50));
            s.set_value(i * 10);
        });

        auto result = receiver.get_for(std::chrono::milliseconds(200));
        if (result)
            std::cout << "Got: " << *result << "\n";
        else
            std::cout << "Timeout!\n";

        sender.reset();
        receiver.reset();
        worker.join();
    }
}
```

