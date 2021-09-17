import SwiftUI
import Foundation
import UIKit
import CoreBluetooth
import Combine


struct IMU_Payload {
    var Qx:Float32?
    var Qy:Float32?
    var Qz:Float32?
    var Real:Float32?
    var Ax:Float32?
    var Ay:Float32?
    var Az:Float32?
    var SampleCount:UInt32?
    
   }


//fills struct with data bytes received
func dataToStruct(data: Data) -> IMU_Payload {
    let _data = data
//    var IMUD:IMU_Payload
    //memcpy(&IMUD,_data,_data.count)
    let converted:IMU_Payload = _data.withUnsafeBytes { $0.load(as: IMU_Payload.self) }
    return converted
  }

func bytesToFloat(bytes b: [UInt8]) -> Float {
    let bigEndianValue = b.withUnsafeBufferPointer {
        $0.baseAddress!.withMemoryRebound(to: UInt32.self, capacity: 1) { $0.pointee }
    }
    let bitPattern = UInt32(bigEndian: bigEndianValue)
  
    return Float(bitPattern: bitPattern)
}

open class BLEConnection: NSObject, CBPeripheralDelegate, CBCentralManagerDelegate, ObservableObject {
    
  static var shared = BLEConnection()
  @EnvironmentObject var data: DataModel
  
  // Properties
  private var centralManager: CBCentralManager! = nil
  private var peripheral: CBPeripheral!

  let SA_SERVICE_UUID = "D587BB16-6DAC-4BC3-B3EE-601B2E72C880"
  public static let bleServiceUUID = CBUUID.init(string: "D587BB16-6DAC-4BC3-B3EE-601B2E72C880")
  public static let bleCharacteristicUUID = CBUUID.init(string: "1DD852AD-7CBD-4A54-875A-CE3D10F9340A")
  var charDictionary = [String: CBCharacteristic]()
  
  
  // Array to contain names of BLE devices to connect to.
  // Accessable by ContentView for Rendering the SwiftUI Body on change in this array.
  struct BLEDevice: Identifiable {
    let id: String
    let name: String
  }
  @Published var scannedBLEDevices: [BLEDevice] = []
  @Published var scannedBLENames: [BLEDevice] = []
    @Published var IMUSamples = IMU_Payload(Ax:0.0,Ay:0.0,Az:0.0,SampleCount:0)
  //@Published var showAlert: Bool = false
  
  func startCentralManager() {
    self.centralManager = CBCentralManager(delegate: self, queue: nil)
    print("Central Manager State: \(self.centralManager.state)")
    DispatchQueue.main.asyncAfter(deadline: .now() + 1) {
      self.centralManagerDidUpdateState(self.centralManager)
    }
  }
  
  // Handles BT Turning On/Off
  public func centralManagerDidUpdateState(_ central: CBCentralManager) {
    switch (central.state) {
    case .unsupported:
      print("BLE is Unsupported")
      break
    case .unauthorized:
      print("BLE is Unauthorized")
      break
    case .unknown:
      print("BLE is Unknown")
      break
    case .resetting:
      print("BLE is Resetting")
      break
    case .poweredOff:
      print("BLE is Powered Off")
      break
    case .poweredOn:
      print("Central scanning for", BLEConnection.bleServiceUUID);
      //self.centralManager.scanForPeripherals(withServices:nil)
      //let services:[CBUUID] = [CBUUID(string: SA_SERVICE_UUID)]
      self.centralManager.scanForPeripherals(withServices: nil, options: nil)
        
        //let services:[CBUUID] = [CBUUID(string: SA_SERVICE_UUID)]
        //self.centralManager.scanForPeripherals(withServices: services,options: [CBCentralManagerScanOptionAllowDuplicatesKey :true])
      break
    @unknown default:
      print("BLE is Unknown")
    }
    
//    if(central.state != CBManagerState.poweredOn)
//    {
//      // In a real app, you'd deal with all the states correctly
//      return;
//    }
  }
  
  var deviceID0: Int = 0
  var deviceID1: Int = 0
  
  // Handles the result of the scan
  public func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String : Any], rssi RSSI: NSNumber) {
    print("Peripheral Name: \(String(describing: peripheral.name))  RSSI: \(String(RSSI.doubleValue))")
    // We've found it so stop scan
    //self.centralManager.stopScan()
    // Copy the peripheral instance
    self.peripheral = peripheral
    let localName:String
    if advertisementData[CBAdvertisementDataLocalNameKey] != nil {
      localName = advertisementData[CBAdvertisementDataLocalNameKey] as! String
      print("Local Name:  \(localName)" )
      if localName.contains("IMUBox") {
        // We've found it so stop scan
        self.centralManager.stopScan()
        
        print("\(localName) is found")
        self.scannedBLENames.append(BLEDevice(id:String(deviceID0), name:localName))
        deviceID0 = deviceID0 + 1
        
        if let safeName = peripheral.name {
          self.scannedBLEDevices.append(BLEDevice(id:String(deviceID1), name:safeName))
          deviceID1 = deviceID1 + 1
        }
        
        self.peripheral.delegate = self
        // Connect!
        self.centralManager.connect(self.peripheral, options: nil)
      }
      
    }
    else {
      localName = "No Local Name"
      print("No Local Name")
    }
    
//    if let safeName = peripheral.name {
//      self.scannedBLEDevices.append(BLEDevice(id:String(deviceID1), name:safeName))
//      deviceID1 = deviceID1 + 1
//      //self.peripheral.delegate = self
//      // Connect!
//      //self.centralManager.connect(self.peripheral, options: nil)
//    }
  }
  
  
  // The handler if we do connect successfully
  public func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
    if peripheral == self.peripheral {
      print("Connected to your BLE Board")
      peripheral.discoverServices([BLEConnection.bleServiceUUID])
      //peripheral.discoverServices(nil)
    }
  }
  
  
  // Handles discovery event
  public func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
    if let services = peripheral.services {
      for service in services {
        print("Found services UUID: \(service.uuid), uuidstring:\(service.uuid.uuidString)")
      
        
        if service.uuid == BLEConnection.bleServiceUUID {
          print("BLE Service found")
          //Now kick off discovery of characteristics
          peripheral.discoverCharacteristics([BLEConnection.bleCharacteristicUUID], for: service)
          return
        }
      }
    }
  }
  
  // Handling discovery of characteristics
  public func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
    if let characteristics = service.characteristics {
      for characteristic in characteristics {
        let uuidString = characteristic.uuid.uuidString
        charDictionary[uuidString] = characteristic
        print("characteristic: \(uuidString)")
        print(characteristic)
        peripheral.setNotifyValue(true, for: characteristic)
        //peripheral.readValue(for: characteristic)
        //        if characteristic.uuid == BLEConnection.bleCharacteristicUUID {
        //          print("BLE service characteristic \(BLEConnection.bleCharacteristicUUID) found")
        //        } else {
        //          print("Characteristic not found.")
        //        }
      }
    }
  }
  
  public func peripheral(_ peripheral: CBPeripheral, didUpdateNotificationStateFor characteristic: CBCharacteristic, error: Error?) {
    if error == nil {
      print("Notification Set OK, isNotifying: \(characteristic.isNotifying)")
      if !characteristic.isNotifying {
        print("isNotifying is false, set to true again!")
        peripheral.setNotifyValue(true, for: characteristic)
      }
    }
  }
  


  public func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
    
    if characteristic.uuid.uuidString == "1DD852AD-7CBD-4A54-875A-CE3D10F9340A" {
      let mydata = characteristic.value!
      //let string = "> " + String(data: data as Data, encoding: .utf8)!
      for i in 0 ... 31 {
        print("\(i):\(mydata[i])", terminator:" ")
      }
      print("")
      //print("struct size\(sizeof(IMUSamples.size))")
        //IMUSamples = dataToStruct(data: mydata)
        
        self.IMUSamples.Qx=mydata.withUnsafeBytes { $0.load(as: Float.self) }
        self.IMUSamples.Qy=mydata.withUnsafeBytes { $0.load(fromByteOffset: 4, as: Float.self)}
        self.IMUSamples.Qz=mydata.withUnsafeBytes { $0.load(fromByteOffset: 8, as: Float.self)}
        self.IMUSamples.Real=mydata.withUnsafeBytes { $0.load(fromByteOffset: 12, as: Float.self)}
        self.IMUSamples.Ax=mydata.withUnsafeBytes { $0.load(fromByteOffset: 16, as: Float.self)}
        self.IMUSamples.Ay=mydata.withUnsafeBytes { $0.load(fromByteOffset: 20, as: Float.self)}
        self.IMUSamples.Az=mydata.withUnsafeBytes { $0.load(fromByteOffset: 24, as: Float.self)}
        self.IMUSamples.SampleCount = mydata.withUnsafeBytes { $0.load(fromByteOffset:28, as: UInt32.self)}
        //self.data.xTranslation=Float(self.IMUSamples.Ax ?? 0.0)
        //$data.yTranslation=Float(self.IMUSamples.Ay ?? 0.0)
        //$data.zTranslation=Float(self.IMUSamples.Az ?? 0.0)

        
        
        //print("Ax=\(self.IMUSamples.Ax!),Ay=\(self.IMUSamples.Ay!),Az=\(self.IMUSamples.Az!),Cnt=\(self.IMUSamples.SampleCount!)")
        print("pkt size: \(mydata.count)")
        let ssize=MemoryLayout<IMU_Payload>.size
        print("struct size: \(ssize)")
        
       
        //let value = bytesToFloat(mydata.)
        //print(value) //->1.0
        //IMUSamples
      
    }
  }
  
}
