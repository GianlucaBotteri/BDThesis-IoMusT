#include "inet/applications/base/ApplicationPacket_m.h"
#include "inet/applications/udpapp/myUdpBasicBurst.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/TimeTag_m.h"
#include "inet/common/packet/Packet.h"
#include "inet/networklayer/common/FragmentationTag_m.h"
#include "inet/networklayer/common/L3AddressResolver.h"
#include "inet/transportlayer/contract/udp/UdpControlInfo_m.h"
#include <random>

namespace inet {

EXECUTE_ON_STARTUP(
        cEnum * e = cEnum::find("inet::ChooseDestAddrMode");
        if (!e)
            enums.getInstance()->add(e = new cEnum("inet::ChooseDestAddrMode"));
        e->insert(myUdpBasicBurst::ONCE, "once");
        e->insert(myUdpBasicBurst::PER_BURST, "perBurst");
        e->insert(myUdpBasicBurst::PER_SEND, "perSend");
        );

Define_Module(myUdpBasicBurst);

simsignal_t myUdpBasicBurst::outOfOrderPkSignal = registerSignal("outOfOrderPk");

myUdpBasicBurst::~myUdpBasicBurst()
{
    cancelAndDelete(timerNext);
}

void myUdpBasicBurst::initialize(int stage)
{
    ApplicationBase::initialize(stage);

    if (stage == INITSTAGE_LOCAL) {
        numSent = 0;
        numReceived = 0;
        numDeleted = 0;
        numDuplicated = 0;

        delayLimit = par("delayLimit");
        startTime = par("startTime");
        stopTime = par("stopTime");
        if (stopTime >= SIMTIME_ZERO && stopTime <= startTime)
            throw cRuntimeError("Invalid startTime/stopTime parameters");

        messageLengthPar = &par("messageLength");
        burstDurationPar = &par("burstDuration");
        sleepDurationPar = &par("sleepDuration");
        sendIntervalPar = &par("sendInterval");

        barrStreakPar = &par("barrStreak");
        skillsLevelPar = &par("skillsLevel");

        nextSleep = startTime;
        nextBurst = startTime;
        nextPkt = startTime;
        dontFragment = par("dontFragment");

        destAddrRNG = par("destAddrRNG");
        const char *addrModeStr = par("chooseDestAddrMode");
        int addrMode = cEnum::get("inet::ChooseDestAddrMode")->lookup(addrModeStr);
        if (addrMode == -1)
            throw cRuntimeError("Invalid chooseDestAddrMode: '%s'", addrModeStr);
        chooseDestAddrMode = static_cast<ChooseDestAddrMode>(addrMode);

        WATCH(numSent);
        WATCH(numReceived);
        WATCH(numDeleted);
        WATCH(numDuplicated);

        localPort = par("localPort");
        destPort = par("destPort");

        timerNext = new cMessage("UDPBasicBurstTimer");
    }
}

L3Address myUdpBasicBurst::chooseDestAddr()
{
    if (destAddresses.size() == 1)
        return destAddresses[0];

    int k = getRNG(destAddrRNG)->intRand(destAddresses.size());
    return destAddresses[k];
}

Packet *myUdpBasicBurst::createPacket()
{
    char msgName[64];
    simtime_t sendTime = simTime(); //SimulationTime alla creazione del pacchetto
    std::string strSendTime = std::to_string(sendTime.dbl()); 
    //Invia il valore simTime di invio come titolo del pacchetto
    sprintf(msgName, "%s", strSendTime.c_str());

    long msgByteLength = *messageLengthPar;
    Packet *pk = new Packet(msgName);
    const auto& payload = makeShared<ApplicationPacket>();
    payload->setChunkLength(B(msgByteLength));
    payload->setSequenceNumber(numSent);
    payload->addTag<CreationTimeTag>()->setCreationTime(simTime());
    pk->insertAtBack(payload);
    pk->addPar("sourceId") = getId();
    pk->addPar("msgId") = numSent;
    return pk;
}

void myUdpBasicBurst::processStart()
{
    socket.setOutputGate(gate("socketOut"));
    socket.setCallback(this);
    socket.bind(localPort);

    int timeToLive = par("timeToLive");
    if (timeToLive != -1)
        socket.setTimeToLive(timeToLive);

    int dscp = par("dscp");
    if (dscp != -1)
        socket.setDscp(dscp);

    int tos = par("tos");
    if (tos != -1)
        socket.setTos(tos);

    const char *destAddrs = par("destAddresses");
    cStringTokenizer tokenizer(destAddrs);
    const char *token;
    bool excludeLocalDestAddresses = par("excludeLocalDestAddresses");

    IInterfaceTable *ift = getModuleFromPar<IInterfaceTable>(par("interfaceTableModule"), this);

    while ((token = tokenizer.nextToken()) != nullptr) {
        if (strstr(token, "Broadcast") != nullptr)
            destAddresses.push_back(Ipv4Address::ALLONES_ADDRESS);
        else {
            L3Address addr = L3AddressResolver().resolve(token);
            if (excludeLocalDestAddresses && ift && ift->isLocalAddress(addr))
                continue;
            destAddresses.push_back(addr);
        }
    }

    nextSleep = simTime();
    nextBurst = simTime();
    nextPkt = simTime();
    activeBurst = false;

    isSource = !destAddresses.empty();

    if (isSource) {
        if (chooseDestAddrMode == ONCE)
            destAddr = chooseDestAddr();

        activeBurst = true;
    }
    timerNext->setKind(SEND);
    processSend();
}

void myUdpBasicBurst::processSend()
{
    if (stopTime < SIMTIME_ZERO || simTime() < stopTime) {
        // send and reschedule next sending
        if (isSource) // if the node is a sink, don't generate messages
            generateBurst();
    }
}

void myUdpBasicBurst::processStop()
{
    socket.close();
    socket.setCallback(nullptr);
}

void myUdpBasicBurst::handleMessageWhenUp(cMessage *msg)
{
    if (msg->isSelfMessage()) {
        switch (msg->getKind()) {
            case START:
                processStart();
                break;

            case SEND:
                processSend();
                break;

            case STOP:
                processStop();
                break;

            default:
                throw cRuntimeError("Invalid kind %d in self message", (int)msg->getKind());
        }
    }
    else
        socket.processMessage(msg);
}

void myUdpBasicBurst::socketDataArrived(UdpSocket *socket, Packet *packet)
{
    std::string pcktName = std::string(packet->getName());
    std::string pcktErrInfo = pcktName.substr(0, pcktName.find(":", 0));
    std::string pcktDelayInfo = pcktName.substr(pcktName.find(":", 0)+1, pcktName.length());

    double error = std::stod(pcktErrInfo);
    double delay = std::stod(pcktDelayInfo);

    sendIntervalPar->setDoubleValue(1);

    std::random_device rDevice;  //Will be used to obtain a seed for the random number engine
    std::mt19937 gen(rDevice()); //Standard mersenne_twister_engine seeded with rd()

    double stdDev = delay/skillsLevelPar->doubleValue();

    std::normal_distribution<double> normD{0, stdDev};
    sendIntervalPar->setDoubleValue(sendIntervalPar->doubleValue()+normD(gen));

    if (numSent > 1 && numSent % barrStreakPar->intValue() == 0){  
	//Simula errore del performer (distrazione)
        std::normal_distribution<double> normD1{1, 0.005};
        sendIntervalPar->setDoubleValue(normD1(gen));
    }
    // process incoming packet
    processPacket(packet);
}

void myUdpBasicBurst::socketErrorArrived(UdpSocket *socket, Indication *indication)
{
    EV_WARN << "Ignoring UDP error report " << indication->getName() << endl;
    delete indication;
}

void myUdpBasicBurst::socketClosed(UdpSocket *socket)
{
    if (operationalState == State::STOPPING_OPERATION)
        startActiveOperationExtraTimeOrFinish(par("stopOperationExtraTime"));
}

void myUdpBasicBurst::refreshDisplay() const
{
    ApplicationBase::refreshDisplay();

    char buf[100];
    sprintf(buf, "rcvd: %d pks\nsent: %d pks", numReceived, numSent);
    getDisplayString().setTagArg("t", 0, buf);
}

void myUdpBasicBurst::processPacket(Packet *pk)
{
    if (pk->getKind() == UDP_I_ERROR) {
        EV_WARN << "UDP error received\n";
        delete pk;
        return;
    }

    if (pk->hasPar("sourceId") && pk->hasPar("msgId")) {
        // duplicate control
        int moduleId = pk->par("sourceId");
        int msgId = pk->par("msgId");
        auto it = sourceSequence.find(moduleId);
        if (it != sourceSequence.end()) {
            if (it->second >= msgId) {
                EV_DEBUG << "Out of order packet: " << UdpSocket::getReceivedPacketInfo(pk) << endl;
                emit(outOfOrderPkSignal, pk);
                delete pk;
                numDuplicated++;
                return;
            }
            else
                it->second = msgId;
        }
        else
            sourceSequence[moduleId] = msgId;
    }

    if (delayLimit > 0) {
        if (simTime() - pk->getTimestamp() > delayLimit) {
            EV_DEBUG << "Old packet: " << UdpSocket::getReceivedPacketInfo(pk) << endl;
            PacketDropDetails details;
            details.setReason(CONGESTION);
            emit(packetDroppedSignal, pk, &details);
            delete pk;
            numDeleted++;
            return;
        }
    }

    EV_INFO << "Received packet: " << UdpSocket::getReceivedPacketInfo(pk) << endl;
    emit(packetReceivedSignal, pk);
    numReceived++;
    delete pk;
}

void myUdpBasicBurst::generateBurst()
{
    simtime_t now = simTime();

    if (nextPkt < now)
        nextPkt = now;

    double sendInterval = *sendIntervalPar;
    if (sendInterval <= 0.0)
        throw cRuntimeError("The sendInterval parameter must be bigger than 0");
    nextPkt += sendInterval;

    if (activeBurst && nextBurst <= now) {    // new burst
        double burstDuration = *burstDurationPar;
        if (burstDuration < 0.0)
            throw cRuntimeError("The burstDuration parameter mustn't be smaller than 0");
        double sleepDuration = *sleepDurationPar;

        if (burstDuration == 0.0)
            activeBurst = false;
        else {
            if (sleepDuration < 0.0)
                throw cRuntimeError("The sleepDuration parameter mustn't be smaller than 0");
            nextSleep = now + burstDuration;
            nextBurst = nextSleep + sleepDuration;
        }

        if (chooseDestAddrMode == PER_BURST)
            destAddr = chooseDestAddr();
    }

    if (chooseDestAddrMode == PER_SEND)
        destAddr = chooseDestAddr();

    Packet *payload = createPacket();
    if(dontFragment)
        payload->addTag<FragmentationReq>()->setDontFragment(true);
    payload->setTimestamp();
    emit(packetSentSignal, payload);
    socket.sendTo(payload, destAddr, destPort);
    numSent++;

    // Next timer
    if (activeBurst && nextPkt >= nextSleep)
        nextPkt = nextBurst;

    if (stopTime >= SIMTIME_ZERO && nextPkt >= stopTime) {
        timerNext->setKind(STOP);
        nextPkt = stopTime;
    }
    scheduleAt(nextPkt, timerNext);
}

void myUdpBasicBurst::finish()
{
    recordScalar("Total sent", numSent);
    recordScalar("Total received", numReceived);
    recordScalar("Total deleted", numDeleted);
    ApplicationBase::finish();
}

void myUdpBasicBurst::handleStartOperation(LifecycleOperation *operation)
{
    simtime_t start = std::max(startTime, simTime());

    if ((stopTime < SIMTIME_ZERO) || (start < stopTime) || (start == stopTime && startTime == stopTime)) {
        timerNext->setKind(START);
        scheduleAt(start, timerNext);
    }
}

void myUdpBasicBurst::handleStopOperation(LifecycleOperation *operation)
{
    if (timerNext)
        cancelEvent(timerNext);
    activeBurst = false;
    socket.close();
    delayActiveOperationFinish(par("stopOperationTimeout"));
}

void myUdpBasicBurst::handleCrashOperation(LifecycleOperation *operation)
{
    if (timerNext)
        cancelEvent(timerNext);
    activeBurst = false;
    if (operation->getRootModule() != getContainingNode(this))     // closes socket when the application crashed only
        socket.destroy();         //TODO  in real operating systems, program crash detected by OS and OS closes sockets of crashed programs.
}

} // namespace inet

