//
//  ContentView.swift
//  testBox
//
//  Created by Joshua Karch on 9/16/21.
//

import SwiftUI
import RealityKit

struct ContentView : View {
    @EnvironmentObject var data: DataModel
    @EnvironmentObject var ble: BLEConnection
    
    var body: some View {
        HStack {
            //ARUIView()
            
            if data.enableAR {ARDisplayView()}
            else {Spacer()}
        }
        
         Text("Measurements").font(.title).foregroundColor(.red)
         let qx:Float32=ble.IMUSamples.Qx ?? 0.0
         let qy:Float32=ble.IMUSamples.Qy ?? 0.0
         let qz:Float32=ble.IMUSamples.Qz ?? 0.0
         let real:Float32=ble.IMUSamples.Real ?? 0.0
         let accx:Float32=ble.IMUSamples.Ax ?? 0.0
         let accy:Float32=ble.IMUSamples.Ay ?? 0.0
         let accz:Float32=ble.IMUSamples.Az ?? 0.0
         let cnt:UInt32=ble.IMUSamples.SampleCount ?? 0
         Text("Ax \(accx)")
             .foregroundColor(.red)
         Text("Ay \(accy)")
             .foregroundColor(.green)
         Text("Az \(accz)")
             .foregroundColor(.blue)
         Text("Count \(cnt)")
             .foregroundColor(.gray)
        
        

        .onAppear(perform: connectBLEDevice)
        .onChange(of: accx, perform: { value in
            setIMU(Qx:qx,Qy:qy,Qz:qz,Real:real,x:accx,y:accy,z:accz)
        })
        
    }
    
    
    
    private func connectBLEDevice(){
      // Start Scanning for BLE Devices
      //ble.showAlert = true
        data.enableAR = true
      ble.startCentralManager()
        
    }
    private func setIMU(Qx:Float32,Qy:Float32,Qz:Float32,Real:Float32,x:Float32,y:Float32,z:Float32) {
        data.xTranslation=Float(-x)*0.1
        data.yTranslation=Float(-z)*0.1
        data.zTranslation=Float(y)*0.1
        data.Qx = 1.0 * Qx
        data.Qy = 1.0 * Qz
        data.Qz = -1.0 * Qy
        data.Real=Real
    }
}


#if DEBUG
struct ContentView_Previews : PreviewProvider {
    static var previews: some View {
        ContentView()
    }
}
#endif
