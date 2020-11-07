#include "inet/applications/udpapp/myUdpEchoApp.h"
#include "inet/common/ModuleAccess.h"
#include "inet/networklayer/common/L3AddressTag_m.h"
#include "inet/transportlayer/common/L4PortTag_m.h"
#include "inet/transportlayer/contract/udp/UdpControlInfo_m.h"
#include <unistd.h>

namespace inet {

Define_Module(myUdpEchoApp);

void myUdpEchoApp::initialize(int stage)
{
    ApplicationBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {

        serverDelayPar = &par("serverDelay");

        // init statistics
        numEchoed = 0;
        barrError = 0;
        barrErrorRate=0;
        WATCH(numEchoed);
        WATCH(barrErrorRate);

    }
}

void myUdpEchoApp::handleMessageWhenUp(cMessage *msg)
{
    socket.processMessage(msg);
}

void myUdpEchoApp::socketDataArrived(UdpSocket *socket, Packet *pk)
{
    simtime_t arrivalTime = simTime();
    double apprxArrTime = std::round(arrivalTime.dbl());
    double error = arrivalTime.dbl() - apprxArrTime - 0.002;

    if (std::fabs(error) >= eps){
        barrError++;
    }

    char msgName[64];
    std::string strErrDelay = std::to_string(error) + ":" 
	+ std::to_string(serverDelayPar->doubleValue());
    sprintf(msgName, "%s", strErrDelay.c_str());

    pk->setName(msgName);

    // determine its source address/port
    L3Address remoteAddress = pk->getTag<L3AddressInd>()->getSrcAddress();
    int srcPort = pk->getTag<L4PortInd>()->getSrcPort();
    pk->clearTags();
    pk->trim();

    numEchoed++;
    emit(packetSentSignal, pk);
    // send back
    socket->sendTo(pk, remoteAddress, srcPort);
}

void myUdpEchoApp::socketErrorArrived(UdpSocket *socket, Indication *indication)
{
    EV_WARN << "Ignoring UDP error report " << indication->getName() << endl;
    delete indication;
}

void myUdpEchoApp::socketClosed(UdpSocket *socket)
{
    if (operationalState == State::STOPPING_OPERATION)
        startActiveOperationExtraTimeOrFinish(par("stopOperationExtraTime"));
}

void myUdpEchoApp::refreshDisplay() const
{
    ApplicationBase::refreshDisplay();

    char buf[40];
    sprintf(buf, "echoed: %d pks", numEchoed);
    getDisplayString().setTagArg("t", 0, buf);
}

void myUdpEchoApp::finish()
{
    barrErrorRate= double(barrError)/double(numEchoed);
    recordScalar("BarrErrorRate", barrErrorRate);
    ApplicationBase::finish();
}

void myUdpEchoApp::handleStartOperation(LifecycleOperation *operation)
{
    socket.setOutputGate(gate("socketOut"));
    int localPort = par("localPort");
    socket.bind(localPort);
    MulticastGroupList mgl = getModuleFromPar<IInterfaceTable>(par("interfaceTableModule"), this)->collectMulticastGroups();
    socket.joinLocalMulticastGroups(mgl);
    socket.setCallback(this);
}

void myUdpEchoApp::handleStopOperation(LifecycleOperation *operation)
{
    socket.close();
    delayActiveOperationFinish(par("stopOperationTimeout"));
}

void myUdpEchoApp::handleCrashOperation(LifecycleOperation *operation)
{
    if (operation->getRootModule() != getContainingNode(this))     // closes socket when the application crashed only
        socket.destroy();         //TODO  in real operating systems, program crash detected by OS and OS closes sockets of crashed programs.
    socket.setCallback(nullptr);
}

} // namespace inet

