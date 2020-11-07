#ifndef __INET_MYUDPECHOAPP_H
#define __INET_MYUDPECHOAPP_H

#include "inet/common/INETDefs.h"

#include "inet/applications/base/ApplicationBase.h"
#include "inet/transportlayer/contract/udp/UdpSocket.h"

namespace inet {

class INET_API myUdpEchoApp : public ApplicationBase, public UdpSocket::ICallback
{
  protected:
    UdpSocket socket;
    int numEchoed;

    int barrError; //Counter delle battute sbagliate
    double barrErrorRate; //Frazione delle battute sbagliate sul totale BER = barrError/numEchoed

    const double eps = 0.02; //Tollerenza per segnare una battuta sbagliata oppure giuste

    cPar *serverDelayPar = nullptr;

  protected:
    virtual int numInitStages() const override { return NUM_INIT_STAGES; }
    virtual void initialize(int stage) override;
    virtual void handleMessageWhenUp(cMessage *msg) override;
    virtual void finish() override;
    virtual void refreshDisplay() const override;

    virtual void handleStartOperation(LifecycleOperation *operation) override;
    virtual void handleStopOperation(LifecycleOperation *operation) override;
    virtual void handleCrashOperation(LifecycleOperation *operation) override;

    virtual void socketDataArrived(UdpSocket *socket, Packet *packet) override;
    virtual void socketErrorArrived(UdpSocket *socket, Indication *indication) override;
    virtual void socketClosed(UdpSocket *socket) override;
};

} // namespace inet

#endif // ifndef __INET_UDPECHOAPP_H

