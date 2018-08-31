
Stromversorgung

Master hat ein Netzteil mit 9V und versorgt alle Boxen mit Spannung an RAW.


Master mit 2 aktiven Boxen ohne Display: 140mA bei 9V
Master mit 4 aktiven Boxen und 20x4 Display: 180mA bei 9V



Bei der 2. Box aus Zweig A war die 9V Arduino-Raw-Spannung verpolt. Ist geändert in der Box hinter der Netzwerkbuchse.
Bei der gleichen Box hatte die 240V Spannung einen Wackelkontakt.
Zweig A+B können nun vertauscht werden (getestet).

Bei Programmieren des Master Zweig A entfernen



Box: In der drive-Funktion
  digitalWrite(STEP_PIN, HIGH);
  delayMicroseconds(10);   //was 10
  digitalWrite(STEP_PIN, LOW);
  delayMicroseconds(10);   //was 10
ist es unerheblich, ob das delay bei beiden gleich ist. Das Zittern ändert sich nicht.


NEXT:
Die 9V nur 1x durchs Netzwerkkabel schleifen und 2 Pins für einen Interrupt nutzen.
So kann eine Notabschaltung realisiert werden.



# Motortreiber
schwarz: 2100
grün: 2130

Bei den 2130ern müssen auf der Unterseite die Jumper geschlossen werden, um sie zu benutzen (und nicht per SPI zu programmieren).
2130: https://github.com/watterott/SilentStepStick/blob/master/hardware/SilentStepStick-TMC2130_v11.pdf




Motortreiber:
http://www.watterott.com/de/SilentStepStick
https://ultimaker.com/en/community/11571-step-by-step-installation-of-silentstepstick-drivers-on-umo?page=1

Protector:
http://www.watterott.com/de/SilentStepStick-Protector

Motor:
https://eckstein-shop.de/Pololu-Stepper-Motor-NEMA-23-Unipolar-Bipolar-200-Steps-Rev-5756mm-74V-1-A-Phase?curr=EUR&gclid=CjwKCAjw4KvPBRBeEiwAIqCB-WNX1xW5KmgovC5qiYSIwEqdvOSpQ0-2ULu6E0rG2eQhQs-N9WBvKhoCQG0QAvD_BwE

Und Martins Empfehlung für einen Sicherheitswiderstand:

der Widerstand kommt vor die Logik-Versorgungsspannung des Motortreibers
(also in die 5V-Leitung). Ich weiß nicht mehr genau, wie groß der
widerstand in meinem testaufbau war - aber irgendwas um 150 bis 200 ohm
sollte gut gehen. Und damit der Treiber nicht mal zwischendurch absäuft
wenn er mal kurz etwas mehr strom braucht, schadet auch ein kondensator
nicht, der dann noch mal parallel zum motortreiber an den 5V hängt.
(hatte da irgendwas um 47 oder 100 uF drin, so weit ich mich erinnere).
