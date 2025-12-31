#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include <flecs.h>

class FlecsResetListener : public Catch::EventListenerBase {
public:
    using Catch::EventListenerBase::EventListenerBase;

    void testCaseStarting(Catch::TestCaseInfo const&) override {
        flecs::reset();
    }
};

CATCH_REGISTER_LISTENER(FlecsResetListener)
