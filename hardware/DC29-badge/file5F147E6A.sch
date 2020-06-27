EESchema Schematic File Version 4
EELAYER 30 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 3 5
Title "DC29 Badge"
Date "06/01/2020"
Rev "1"
Comp "Team IDES"
Comment1 "SemTech SX1262 LoRa radio"
Comment2 "RGB 320x240 TFT display"
Comment3 "STM32F746 CPU with 8MB external SDRAM"
Comment4 "\"Roaring 20s\" prototype design"
$EndDescr
$Comp
L ER-CON50HT-1:ER-CON50HT-1 J1
U 1 1 5EDD544D
P 1700 4100
F 0 "J1" H 1593 1233 50  0000 C CNN
F 1 "ER-CON50HT-1" H 1593 1324 50  0000 C CNN
F 2 "EASTRISING_ER-CON50HT-1" H 1700 4100 50  0001 L BNN
F 3 "https://www.buydisplay.com/download/manual/ER-TFT024-3_Datasheet.pdf" H 1700 4100 50  0001 L BNN
F 4 "1.0" H 1700 4100 50  0001 L BNN "Field4"
F 5 "Manufacturer Recommendation" H 1700 4100 50  0001 L BNN "Field5"
	1    1700 4100
	-1   0    0    1   
$EndComp
Entry Wire Line
	4500 5300 4600 5200
Wire Wire Line
	4500 5300 2200 5300
Text GLabel 4600 3100 2    50   Input ~ 0
RGB[0..17]
Text Label 4150 5300 0    50   ~ 0
RGB0
Entry Wire Line
	4500 5200 4600 5100
Wire Wire Line
	2200 5200 4500 5200
Entry Wire Line
	4500 5100 4600 5000
Entry Wire Line
	4500 5000 4600 4900
Entry Wire Line
	4500 4900 4600 4800
Entry Wire Line
	4500 4800 4600 4700
Entry Wire Line
	4500 4700 4600 4600
Entry Wire Line
	4500 4600 4600 4500
Entry Wire Line
	4500 4500 4600 4400
Entry Wire Line
	4500 4400 4600 4300
Entry Wire Line
	4500 4300 4600 4200
Entry Wire Line
	4500 4200 4600 4100
Entry Wire Line
	4500 4100 4600 4000
Entry Wire Line
	4500 4000 4600 3900
Wire Wire Line
	2200 5100 4500 5100
Wire Wire Line
	2200 5000 4500 5000
Wire Wire Line
	4500 4900 2200 4900
Wire Wire Line
	4500 4800 2200 4800
Wire Wire Line
	4500 4700 2200 4700
Wire Wire Line
	4500 4600 2200 4600
Wire Wire Line
	2200 4500 4500 4500
Wire Wire Line
	2200 4400 4500 4400
Wire Wire Line
	4500 4300 2200 4300
Wire Wire Line
	4500 4200 2200 4200
Wire Wire Line
	4500 4100 2200 4100
Wire Wire Line
	4500 4000 2200 4000
Entry Wire Line
	4500 3900 4600 3800
Entry Wire Line
	4500 3800 4600 3700
Entry Wire Line
	4500 3700 4600 3600
Entry Wire Line
	4500 3600 4600 3500
Wire Wire Line
	4500 3900 2200 3900
Wire Wire Line
	4500 3800 2200 3800
Wire Wire Line
	4500 3700 2200 3700
Wire Wire Line
	2200 3600 4500 3600
Text Notes 4700 4250 0    50   ~ 0
RGB0-RGB5 = BLUE0-BLUE6\nRGB6-RGB11 = GREEN0-GREEN6\nRGB12-RGB17 = RED0-RED6
Text Label 4150 5200 0    50   ~ 0
RGB1
Text Label 4150 5100 0    50   ~ 0
RGB2
Text Label 4150 5000 0    50   ~ 0
RGB3
Text Label 4150 4900 0    50   ~ 0
RGB4
Text Label 4150 4800 0    50   ~ 0
RGB5
Text Label 4150 4700 0    50   ~ 0
RGB6
Text Label 4150 4600 0    50   ~ 0
RGB7
Text Label 4150 4500 0    50   ~ 0
RGB8
Text Label 4150 4400 0    50   ~ 0
RGB9
Text Label 4150 4300 0    50   ~ 0
RGB10
Text Label 4150 4200 0    50   ~ 0
RGB11
Text Label 4150 4100 0    50   ~ 0
RGB12
Text Label 4150 4000 0    50   ~ 0
RGB13
Text Label 4150 3900 0    50   ~ 0
RGB14
Text Label 4150 3800 0    50   ~ 0
RGB15
Text Label 4150 3700 0    50   ~ 0
RGB16
Text Label 4150 3600 0    50   ~ 0
RGB17
Wire Wire Line
	2200 5700 5100 5700
Wire Wire Line
	5100 5700 5100 5250
Text GLabel 5100 5250 1    50   Input ~ 0
LCD_VSYNC
Wire Wire Line
	2200 5600 5250 5600
Wire Wire Line
	5250 5600 5250 5250
Text GLabel 5250 5250 1    50   Input ~ 0
LCD_HSYNC
Wire Wire Line
	2200 5500 5400 5500
Wire Wire Line
	5400 5500 5400 5250
Wire Wire Line
	2200 5400 5550 5400
Wire Wire Line
	5550 5400 5550 5250
Text GLabel 5400 5250 1    50   Input ~ 0
LCD_CLK
Text GLabel 5550 5250 1    50   Input ~ 0
LCD_DE
Text GLabel 4400 2350 2    50   Input ~ 0
SPI2_[0..2]
Entry Wire Line
	4300 3100 4400 3000
Entry Wire Line
	4300 3000 4400 2900
Entry Wire Line
	4300 2900 4400 2800
Wire Wire Line
	2200 3100 2800 3100
Wire Wire Line
	2800 3100 2800 2900
Wire Wire Line
	2800 2900 4300 2900
Text Label 4000 3100 0    50   ~ 0
SPI2_0
Text Label 4000 3000 0    50   ~ 0
SPI2_1
Text Label 4000 2900 0    50   ~ 0
SPI2_2
Wire Wire Line
	2200 3500 2900 3500
Wire Wire Line
	2900 3500 2900 3000
Wire Wire Line
	2900 3000 4300 3000
Wire Wire Line
	2200 3400 3000 3400
Wire Wire Line
	3000 3400 3000 3100
Wire Wire Line
	3000 3100 4300 3100
Text Notes 4500 2750 0    50   ~ 0
SPI2_0 = MOSI\nSPI2_1 = MISO\nSPI2_2 = SCK
Wire Wire Line
	2200 3000 2700 3000
Wire Wire Line
	2700 3000 2700 2800
Wire Wire Line
	2700 2800 3800 2800
Text GLabel 3800 2800 2    50   Input ~ 0
LCD_CS
$Comp
L Driver_Display:XPT2046TS U3
U 1 1 5F4488E8
P 8800 2300
F 0 "U3" H 8800 1711 50  0000 C CNN
F 1 "XPT2046TS" H 8800 1620 50  0000 C CNN
F 2 "Package_SO:TSSOP-16_4.4x5mm_P0.65mm" H 8800 1700 50  0001 C CIN
F 3 "http://www.xptek.cn/uploadfile/download/201707171401161883.pdf" H 9000 1750 50  0001 C CNN
	1    8800 2300
	1    0    0    -1  
$EndComp
Wire Wire Line
	2200 2400 3600 2400
Wire Wire Line
	3600 2400 3600 1350
Wire Wire Line
	3600 1350 7500 1350
Wire Wire Line
	7500 1350 7500 2000
Wire Wire Line
	7500 2000 8300 2000
Wire Wire Line
	2200 2300 3700 2300
Wire Wire Line
	3700 2300 3700 1450
Wire Wire Line
	3700 1450 7400 1450
Wire Wire Line
	7400 1450 7400 2100
Wire Wire Line
	7400 2100 8300 2100
Wire Wire Line
	2200 2200 3800 2200
Wire Wire Line
	3800 2200 3800 1550
Wire Wire Line
	3800 1550 7300 1550
Wire Wire Line
	7300 1550 7300 2200
Wire Wire Line
	7300 2200 8300 2200
Wire Wire Line
	2200 2100 3900 2100
Wire Wire Line
	3900 2100 3900 1650
Wire Wire Line
	3900 1650 7200 1650
Wire Wire Line
	7200 1650 7200 2300
Wire Wire Line
	7200 2300 8300 2300
Text GLabel 10050 1600 2    50   Input ~ 0
SPI2_[0..1]
Entry Wire Line
	9950 2100 10050 2000
Entry Wire Line
	9950 2000 10050 1900
Entry Wire Line
	9950 2200 10050 2100
Wire Wire Line
	9300 2200 9950 2200
Wire Wire Line
	9300 2100 9950 2100
Wire Wire Line
	9300 2000 9950 2000
Text Label 9700 2000 0    50   ~ 0
SPI2_2
Text Label 9700 2100 0    50   ~ 0
SPI2_1
Text Label 9700 2200 0    50   ~ 0
SPI2_0
Wire Wire Line
	8800 2800 8800 3350
$Comp
L power:GND #PWR0102
U 1 1 5F475B99
P 8800 3350
F 0 "#PWR0102" H 8800 3100 50  0001 C CNN
F 1 "GND" H 8805 3177 50  0000 C CNN
F 2 "" H 8800 3350 50  0001 C CNN
F 3 "" H 8800 3350 50  0001 C CNN
	1    8800 3350
	1    0    0    -1  
$EndComp
Wire Wire Line
	2200 2500 2450 2500
Wire Wire Line
	2450 2500 2450 700 
Wire Wire Line
	2450 700  3300 700 
Wire Wire Line
	3300 700  3300 1000
Wire Wire Line
	2200 2600 2550 2600
Wire Wire Line
	2550 2600 2550 800 
$Comp
L power:GND #PWR0103
U 1 1 5F47B248
P 3300 1000
F 0 "#PWR0103" H 3300 750 50  0001 C CNN
F 1 "GND" H 3305 827 50  0000 C CNN
F 2 "" H 3300 1000 50  0001 C CNN
F 3 "" H 3300 1000 50  0001 C CNN
	1    3300 1000
	1    0    0    -1  
$EndComp
$Comp
L power:+3.3V #PWR0104
U 1 1 5F47B630
P 3600 800
F 0 "#PWR0104" H 3600 650 50  0001 C CNN
F 1 "+3.3V" H 3615 973 50  0000 C CNN
F 2 "" H 3600 800 50  0001 C CNN
F 3 "" H 3600 800 50  0001 C CNN
	1    3600 800 
	1    0    0    -1  
$EndComp
Wire Wire Line
	2550 800  3600 800 
Wire Wire Line
	2200 2700 2550 2700
Wire Wire Line
	2550 2700 2550 2600
Connection ~ 2550 2600
Wire Wire Line
	2200 2800 2550 2800
Wire Wire Line
	2550 2800 2550 2700
Connection ~ 2550 2700
Wire Wire Line
	2200 6200 6650 6200
Wire Wire Line
	6650 6200 6650 5350
Wire Wire Line
	2200 6100 6750 6100
Wire Wire Line
	6750 6100 6750 5450
Wire Wire Line
	6750 5450 8250 5450
Wire Wire Line
	2200 6000 6850 6000
Wire Wire Line
	6850 6000 6850 5550
Wire Wire Line
	6850 5550 8250 5550
Wire Wire Line
	2200 5900 6950 5850
Wire Wire Line
	6950 5850 6950 5650
Wire Wire Line
	6950 5650 8250 5650
Wire Wire Line
	6650 5350 8250 5350
Text Label 7200 5350 0    50   ~ 0
IM0
Text Label 7200 5450 0    50   ~ 0
IM1
Text Label 7200 5550 0    50   ~ 0
IM2
Text Label 7200 5650 0    50   ~ 0
IM3
Wire Bus Line
	4400 2350 4400 3000
Wire Bus Line
	10050 1600 10050 2100
Wire Bus Line
	4600 3100 4600 5200
Text Notes 3700 1100 0    50   ~ 0
Actual display pinout: https://www.buydisplay.com/download/ic/ILI9341.pdf\nNote: May need alternate part: https://www.mouser.com/ProductDetail/Amphenol-FCI/62684-50110E9ALF?qs=%2Fha2pyFadujOUaW3MI47g4tVkznaXoi2XEy9rmwa7FEZzDwWxeWYmQ%3D%3D\nNote: Pins may need to be reversed if "bottom contacts" connector is used.
$EndSCHEMATC
