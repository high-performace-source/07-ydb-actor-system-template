
#include "pg_proxy.h"
#include "pg_listener.h"
#include <library/cpp/actors/core/actor_bootstrapped.h>
#include <library/cpp/actors/interconnect/poller_actor.h>

namespace NPG {

using namespace NActors;

class TPGProxy : public TActorBootstrapped<TPGProxy> {
public:
    TPGProxy()
    {
    }

    void Bootstrap() {
        Poller = Register(CreatePollerActor());
        Listener = Register(CreatePGListener(Poller, {}));
        Become(&TPGProxy::StateWork);
    }

    STATEFN(StateWork) {
        switch (ev->GetTypeRewrite()) {
        }
    }

    TActorId Poller;
    TActorId Listener;
};


NActors::IActor* CreatePGProxy() {
    return new TPGProxy();
}

}