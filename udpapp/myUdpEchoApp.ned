package inet.applications.udpapp;

import inet.applications.contract.IApp;

simple myUdpEchoApp like IApp
{
    parameters:
        double serverDelay @unit(s) = default(0.3s); //Valore per il ritardo del server
        // I valori possibili sono basso=0.05s, medio=0.3s, alto=0.65s
        
        //--------------------- Default INET API NED parameters
        
        string interfaceTableModule;   // The path to the InterfaceTable module
        int localPort;  // local port to listen on
        @display("i=block/app");
        @lifecycleSupport;
        double stopOperationExtraTime @unit(s) = default(-1s);    // extra time after lifecycle stop operation finished
        double stopOperationTimeout @unit(s) = default(2s);    // timeout value for lifecycle stop operation
        @signal[packetSent](type=inet::Packet);
        @statistic[echoedPk](title="packets echoed"; source=packetSent; record=count,"sum(packetBytes)","vector(packetBytes)"; interpolationmode=none);
    gates:
        input socketIn @labels(UdpControlInfo/up);
        output socketOut @labels(UdpControlInfo/down);
}

