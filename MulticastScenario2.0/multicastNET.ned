import inet.node.ethernet.Eth100M;
import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.node.ethernet.Eth10G;
import inet.node.ethernet.EtherSwitch;
import inet.node.inet.MulticastRouter;
import inet.node.inet.Router;
import inet.node.inet.StandardHost;
import inet.node.inet.WirelessHost;
import inet.node.wireless.AccessPoint;
import inet.physicallayer.unitdisk.UnitDiskRadioMedium;
import inet.visualizer.integrated.IntegratedVisualizer;
import ned.DatarateChannel;

channel Eth10GDel extends DatarateChannel
{
    double length @unit(m) = default(10m);
    double serverDelay @unit(s) = default(0.3s); //Valore il ritardo del server, inserito nel canale per caratteristiche Omnet++
    // I valori possibili sono basso=0.05s, medio=0.3s, alto=0.55s
    datarate = 10Gbps;
    delay = replaceUnit(length / 2e8, "s") + serverDelay; 
}


network multicastNetwork
{
    parameters:
        int groupNum;
        volatile int posX = intuniform (750,845);
        volatile int posY = intuniform (75,175);

        @display("bgb=938.6,479.69998;bgg=100,1,grey95");
        @figure[title](type=label; pos=0,-1; anchor=sw; color=darkblue);

    submodules:
        hostDelayed[groupNum]: WirelessHost {
            @display("p=posX,posY");
        }
        hostOnTime[groupNum]: WirelessHost {
            @display("p=posX,posY");
        }
        hostAnticipated[groupNum]: WirelessHost {
            @display("p=posX,posY");
        }
        nRouter: Router {
            @display("p=732.405,163.6725");
            multicastForwarding = true;
        }
        fogServer: StandardHost {
            @display("p=83.2,167.7");
            forwarding = true;
            multicastForwarding = true;
        }
        configurator: Ipv4NetworkConfigurator {
            @display("p=104,35");
        }
        etherSwitch: EtherSwitch {
            @display("p=122.2,245.7");
        }
        backboneRouter: Router {
            @display("p=196.3,409.5;i=abstract/router2");
            multicastForwarding = true;
        }
        accessPoint: AccessPoint {
            @display("p=799.5225,125.9925;r=100");
        }
        visualizer: IntegratedVisualizer {
            @display("p=429,42.899998");
        }
        unitDiskRadioMedium: UnitDiskRadioMedium {
            @display("p=273,41.6");
        }
        serverRouter: MulticastRouter {
            @display("p=206,307");
        }
    connections:
        etherSwitch.ethg$o++ --> Eth10G --> fogServer.ethg$i++;
        etherSwitch.ethg$i++ <-- Eth10GDel <-- fogServer.ethg$o++;
        nRouter.ethg++ <--> Eth10G <--> backboneRouter.ethg++;
        accessPoint.ethg++ <--> Eth100M <--> nRouter.ethg++;
        backboneRouter.ethg++ <--> Eth10G <--> serverRouter.ethg++;
        serverRouter.ethg++ <--> Eth10G <--> etherSwitch.ethg++;
}