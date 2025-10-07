## OneShot.hpp

Single-use one-shot future

### Blocking get with simple value transfer

```
#include "one_shot.hpp"
#include <iostream>
#include <thread>

int main() {
    auto [sender, receiver] = OneShot<int>::make();

    std::thread producer([s = std::move(sender)]() mutable {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        s.set_value(42);
    });

    int val = receiver.get();  // blocks until value is set
    std::cout << "Received value: " << val << std::endl;

    producer.join();
}
```

### Time get with optional value

```
#include "one_shot.hpp"
#include <iostream>
#include <thread>
#include <optional>

int main() {
    auto [sender, receiver] = OneShot<int>::make();

    std::thread producer([s = std::move(sender)]() mutable {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        s.set_value(99);
    });

    if (auto val = receiver.get_for(std::chrono::milliseconds(50))) {
        std::cout << "Got early: " << *val << std::endl;
    } else {
        std::cout << "Timeout, waiting longer..." << std::endl;
        int final_val = receiver.get();
        std::cout << "Got later: " << final_val << std::endl;
    }

    producer.join();
}
```

### Broken promise

```
#include "one_shot.hpp"
#include <iostream>

int main() {
    auto [sender, receiver] = OneShot<int>::make();

    // destroy sender without setting a value
    sender = {};

    try {
        receiver.get();
    } catch (const std::future_error &e) {
        std::cout << "Caught exception: " << e.what() << std::endl;
    }
}
```

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

### void variant

```
#include "one_shot.hpp"
#include <iostream>
#include <thread>

int main() {
    auto [sender, receiver] = OneShot<void>::make();

    std::thread task([s = std::move(sender)]() mutable {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        s.set_value();  // signal completion
    });

    receiver.get();  // waits until signaled
    std::cout << "Task completed!" << std::endl;

    task.join();
}
```

### void variant with broken sender

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

Reusable One shot

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
Output
```
Got: 0
Got: 10
Got: 20
```

### Reusable Channel with void signals

```
#include "one_shot_channel.hpp"
#include <iostream>
#include <thread>

int main() {
    auto [sender, receiver] = OneShotChannel<void>::make();

    for (int i = 0; i < 3; ++i) {
        std::thread t([s = sender]() mutable {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            s.set_value();
        });

        if (receiver.get_for(std::chrono::milliseconds(100)))
            std::cout << "Signal received!\n";

        sender.reset();
        receiver.reset();
        t.join();
    }
}
```
Output
```
Signal received!
Signal received!
Signal received!
```

### Exception Propagation

```
#include "one_shot.hpp"
#include <iostream>
#include <thread>

int main() {
    auto [sender, receiver] = OneShot<int>::make();

    std::thread t([s = std::move(sender)]() mutable {
        s.set_exception(std::make_exception_ptr(std::runtime_error("Something went wrong")));
    });

    try {
        receiver.get();
    } catch (const std::runtime_error &e) {
        std::cout << "Caught runtime error: " << e.what() ZZ<< std::endl;
    }

    t.join();
}
```
