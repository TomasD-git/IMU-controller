# IMU controller  
### **Ultra low power IMU controller, can control anything over bluetooth using IMU, all running on coin cell CR2032 battery**  

<img width="476" height="392" alt="image" src="https://github.com/user-attachments/assets/f24d42b9-22be-4590-b2d1-1ef5e3aa8daf" />

## Features:  
- **Ultra low power 11.5uA power consumption in sleep mode**  
- **Most power consumption(LED on max brightness, cpu and ram fully active, BLE active, IMU running) 16.5mA**
- **Battery lasts 700ish days(11.5µA consumption on a 220mah battery)**
- **Powerfull BLE 5.3 with antenna**
- **NRF52810 as MCU**
- **BMI270 as IMU**
- **CR2032 battery**
- **Red LED**

<details>
<summary>How to does it work</summary>

## The button enables/disables the BLE  
## The LED indicates if BLE is active (LED flashes once = BLE on, LED flashes twice = BLE off)  
## All components are in deep sleep until accel detects movement  
## After few seconds of inactivity goes back to deep sleep  

</details>

<details>
<summary>How to flash</summary>

0. **Connect device to PC using segger j-link**   
1. **Open VS Code**  
2. **Select connected device in left bottom corner:** 
<img width="161" height="132" alt="image" src="https://github.com/user-attachments/assets/4a6b26c8-9f6f-4a38-be94-c9cf9f109e19" />

4. **Flash:**  
<img width="144" height="18" alt="image" src="https://github.com/user-attachments/assets/ed1793c1-9f66-4db3-b07a-b73b643889c2" />    



( For Debugging select: )  
<img width="134" height="24" alt="image" src="https://github.com/user-attachments/assets/3d949833-b146-4a7a-8436-6f0ea4b6ce1a" />  

</details>

<details>
<summary>How to Assemble</summary>

1. **Print all parts and assemble PCB**  
2. **Flash**  
3. **Place the Coin Cell battery into the holder**  
4. **Place the PCB inside the case**  
5. **Screw the bottom lid into the top one**  
6. **Verify it works by clicking the button**  

</details>

<details>  
<summary>Pictures</summary>  
  
<img width="476" height="392" alt="image" src="https://github.com/user-attachments/assets/57d0db0b-6449-4f58-88f1-fc239193a4ed" />
<img width="600" height="489" alt="image" src="https://github.com/user-attachments/assets/ea4e6074-1c51-4514-bdfa-7d2ab7922c64" />
<img width="863" height="290" alt="image" src="https://github.com/user-attachments/assets/a6170143-9a0a-487c-ad18-6511c267e15e" />
<img width="956" height="655" alt="image" src="https://github.com/user-attachments/assets/3aa0ad7f-0b5f-4170-9797-0651436f3fd3" />
<img width="692" height="674" alt="image" src="https://github.com/user-attachments/assets/81b0023a-7e28-4a3d-8c23-dde0c254c8d6" />
<img width="686" height="554" alt="image" src="https://github.com/user-attachments/assets/fb5423d2-89ce-4899-a2d9-eb714e4c82b7" />
<img width="754" height="565" alt="image" src="https://github.com/user-attachments/assets/2e7db618-b5be-44f8-b13a-470b031cb724" />

</details>  

<details>
<summary>BOM</summary>

|Name          |Purpose                                       |Quantity  |Total Cost (USD)|Link                                                                                                                           |Distributor|
|--------------|----------------------------------------------|----------|----------------|-------------------------------------------------------------------------------------------------------------------------------|-----------|
|PCB + stencil |all components are on this                    |1         |13.97           |https://jlcpcb.com                                                                                                             |jlcpcb     |
|ALL components|all components that are on pcb                |1         |38.87           |https://lcsc.com                                                                                                               |lcsc       |
|CR2032        |battery which poweres the entire PCB          |1         |4               |                                                                                                                               |local store|
|SEGGER J-Link |Flashes the NRF board with firmware           |1         |4.94            |https://www.aliexpress.com/item/1005005802567589.html                                                                          |Aliexpress |
|EU tarrifs    |3 EU for each category for orders under 150 eu|3         |32.66           |https://commission.europa.eu/news-and-media/news/ensuring-fairness-and-safety-eur3-customs-duty-low-value-parcels-2026-06-29_en|EU         |
|              |                                              |Total cost|94.44           |                                                                                                                               |           |
|              |If outside EU                                 |Total cost|61.78           |                                                                                                                               |           |

</details>
