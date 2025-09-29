[EN] The MAX30100 sensor is an integrated pulse oximeter and heart rate sensor. With the software you develop, it is possible to calculate both the oxygen saturation level in the blood and the heart rate.

However, the MAX30100 sensor is not a certified medical device; therefore, it must not be used under any circumstances to diagnose or test real medical conditions. Instead, it can be used for developing your own smartwatch, wristband, portable health products, simply for experimenting with microcontrollers. Although the MAX30100 provides a relatively simple data acquisition structure via the I2C protocol, this should not create the impression that accurate data can be obtained immediately. (In fact, this is the main reason I wanted to work with this sensor.) The raw data acquired from the sensor must go through several processing steps, and due to the sensor’s weak calibration capabilities, additional effort is required to obtain reliable results.

Note: I used the board shown in the linked image (https://witcdn.robotistan.com/heart-rate-sensor-max30100-48144-66-B.jpg). This particular board has a hardware issue: the connection between the OUT of the 1.8V regulator and the sensor must be cut, and the resistor should instead be connected to the 3.3V regulator output.

First, we need to acquire raw data. To do this, we must properly initialize the sensor.

```
 ___________       ________       ________       ___________       ________
|           |     |        |     |        |     |           |     |        |
| Processor |---->|  Mode  |---->|  SpO2  |---->| LED Pulse |---->| Delete | 
|   Reset   |     | Select |     | Config |     |  Control  |     |  FIFO  |
|___________|     |________|     |________|     |___________|     |________|
```
After initialization, we can begin reading data from the FIFO. The MAX30100 uses a FIFO structure that makes it straightforward to collect data. The sensor stores measurement data in a 16-depth FIFO. Each sample consists of 4 bytes (2 bytes Red + 2 bytes IR). The data can be read from the FIFO_DATA register at address 0x05. This register is fixed; when a burst read is performed, the next byte in the FIFO is automatically returned. By reading 4 consecutive bytes, a complete sample is obtained. The sensor’s internal FIFO_RD_PTR counter automatically advances to the next sample. Before starting measurements, the FIFO must be cleared by resetting FIFO_WR_PTR, OVF_COUNTER, and FIFO_RD_PTR.

From the sensor, we obtain two raw signals: IR and Red. The first processing step is DC Offset Removal.

As you know, signals from many sensors (microphones, PPG, accelerometers, etc.) are composed of both DC and AC components. The DC component may come from ambient conditions, current levels, or many other factors, and often distorts the signal. The AC component, on the other hand, contains the actual information of interest. For this reason, our goal with DC Offset Removal is to eliminate the offset and extract the informative part of the signal.

When observing the raw output of the MAX30100, you can clearly see a DC offset, as the signal tends to drift slightly. This must be removed.
```
w(t) = x(t) + a * w(t-1)
y(t) = w(t) - w(t-1)
```
Here we use an IIR (Infinite Impulse Response) filter. Since a signal is composed of (DC + AC) components, this recursive equation ensures that as the DC component remains constant, the system output gradually converges to zero, leaving only the AC (information-carrying) component.

```
int cnt = 0;
float x = 894.0f;  // Example input value    
float w = 0.0f;        
float prev_w = 0.0f;   
float alpha = 0.95f;
float y = 0.0f;
	
while (cnt < 300) {
    w = x + alpha * prev_w;   
    y = w - prev_w;           
    prev_w = w;               
    cnt++;
    printf("counter=%d, y=%f\n", cnt, y);
}
```
If you run this code, you will see how the filter operates. With a constant input, the system converges to zero and leaves only the AC component.

After removing the DC offset, the next step is to apply a Mean-Median Filter. This filter reduces small fluctuations while enhancing sudden transitions and peak values. The logic behind the Mean-Median Filter is to smooth average values while emphasizing abrupt changes, such as peaks. It also reduces noise, although in some cases it may increase high-frequency noise. For this reason, a Low-pass Filter (LPF) or Band-pass Filter (BPF) can be designed afterward to refine the signal further.

After these filtering stages, we now have a clean sensor signal. At this stage, peak detection and SpO2 calculation can be performed, and the results can be displayed graphically on a PC. However, although I successfully acquired and filtered the signals, I was not able to achieve the desired quality in the visualization step.

While I learned a great deal throughout this process, I could not produce a final visualization that I could confidently call correct. The most likely reason is the calibration difficulties mentioned in the reference source I used, combined with my own inability to achieve proper calibration. For now, I will share the .csv file containing the recorded data, and later I will explore what steps can be taken to improve visualization and data accuracy.

Source I heavily relied on during this project: https://morf.lv/implementing-pulse-oximeter-using-max30100

MAX30100 Datasheet: https://www.analog.com/media/en/technical-documentation/data-sheets/max30100.pdf

Note: If your computer does not recognize the USB device in USB Device mode, enter via STM32 ST-LINK Utility and perform a firmware update by selecting the device as ST-LINK.

[TR] max30100 sensörü, bir Pulse oksimetre ve kalp atış hızı sensör entegresidir. Oluşturacağınız yazılımlar ile kandaki oksijen miktarını ve kalp atış hızını hesaplayabilirsiniz.

max30100 sensörü onaylı bir cihaz değildir, bu sebeple hiçbir koşulda gerçek bir hastalığı test etmek için kullanılamaz. Kendi akıllı saatini, 
bilekliğini, taşınabilir sağlık ürününü ya da sadece işlemcilerle çalışma yapmak isteyen herkes için kullanılabilir. Max30100 sensörünün, I2C protokolü ile
oldukça basit bir veri okuma yapısına sahip olması, sensörden doğru verinin hemen alınabileceği fikrini ortaya çıkarmamalı.(Neden bu sensörle uğraşmak istediğimin nedeni de bu) Sensörden alınan ham veriyi birçok aşamadan geçirmeli ve kalibrasyon yeteneği zayıf olan sensörün, doğru veriyi alması adına biraz uğraşmalısınız.

NOT:Ben sensörün linki verilen görselde olan boardını kullanıyordum ve donanımsal olarak(https://witcdn.robotistan.com/heart-rate-sensor-max30100-48144-66-B.jpg)
hatalı olan bu sensörü bize yakın olan ile 1.8V regülatördeki OUT arasındaki bağlantı koparılmalı ve direnci, 3.3V çıkış veren regülatöre bağlanmalı.

İlk önce ham veriyi alacağız. Bunun için öncelikle doğru bir init yapmalıyız.

```
 ___________       ________       ________       ___________       ________
|           |     |        |     |        |     |           |     |        |
|  İşlemci  |---->|  Mode  |---->|  SpO2  |---->| LED Pulse |---->| Delete | 
|  Reset    |     | Seçimi |     | Config |     |  Control  |     |  FIFO  |
|___________|     |________|     |________|     |___________|     |________|

```

Initi yaptıktan sonra veriyi FIFO dan okumaya başlayabiliriz. Max30100 sensöründeki FIFO yapısı bize veriyi almamız için oldukça basit bir yapı sunmuştur.
Bu yapıya göre : MAX30100, ölçüm verilerini 16 derinlikli FIFO’da saklar. Her örnek 4 byte’tan oluşur (2 byte Red + 2 byte IR). Veriler 0x05 adresindeki
FIFO_DATA register üzerinden okunur. Bu register sabittir; burst read yapıldığında FIFO’nun bir sonraki byte’ı gelir. Böylece art arda 4 byte okunarak bir 
örnek elde edilir. FIFO’daki sıradaki örneğe geçişi sensörün içindeki FIFO_RD_PTR sayacı otomatik yapar. Ölçüme başlamadan önce FIFO’nun temiz olması için 
FIFO_WR_PTR, OVF_COUNTER ve FIFO_RD_PTR sıfırlanmalıdır.

Sensörden okuduğumuz ve IR, RED adını verdiğimiz iki ham veriye, öncelikle DC Offset Removal uygulayacağız.

Bildiğiniz gibi sensörlerden alınan birçok sinyal(mikrofon, PPG, ivmeölçer, vb...) DC ve AC olmak üzere iki parçadan oluşur. DC bileşen ortamdan, akımdan, 
aklınıza gelebilecek birçok faktörden oluşabilir ve sinyali bozar. AC bileşen ise bilgi kısmıdır. Sinyaller ile gönderilmek istenen bilgi genellikle bu 
parçada gönderilir. 'DC Offset Removal' uygulamamızın sebebi de budur. Sinyaldeki offset den kurtulup, bilgi kaynağına ulaşmak. Max30100 sensörünün ham 
verisi izlendiği zaman bir DC offset olduğu ve grafiğin hafifçe salındığını göreceksiniz. Bundan kurtulmamız gerekmektedir.

```
w(t) = x(t) + a * w(t-1)
y(t) = w(t) - w(t-1)
```
Bunun için üstte verilen IIR(Infinitive Impulse Response) kullanacağız. Bir sinyalin (DC + AC) bileşenlerinden oluştuğunu biliyoruz. Bu denklem sayesinde DC bileşen aynı olduğu sürece sonsuza doğru gidildikçe sistemin sonucu 0'a yaklaşacaktır ve AC(bilgi taşıyan) bileşen kalacaktır.  

```
int cnt = 0;
float x = 894.0f;  //Rastgele bir giris degeri    
float w = 0.0f;        
float prev_w = 0.0f;   
float alpha = 0.95f;
float y = 0.0f;
	
while (cnt < 300) {
    w = x + alpha * prev_w;   
    y = w - prev_w;           
    prev_w = w;               
    cnt++;
    printf("counter=%d, y=%f\n", cnt, y);
}
```
Filtrenin nasıl çalıştığını görmek isterseniz bu kodu çalıştırın. Giriş bileşeni sabit verilir. Sonunda sistem sonucu 0 olur ve sadece AC bileşen kalır.

Elimizde DC offsetten arındırılmış bir sinyal kaldı. Şimdi 'Mean Median Filter' uygulayacağız. Bu filtre ile küçük dalgalanmalar zayıflar ve ani geçişler güçlenir. 'Mean Median Filter' mantığı, ortalama değerleri yumuşatması ve ani değişimleri/peak değerleri öne çıkarmasıdır. Gürültüyü de azaltır fakat yüksek frekanslı gürültüyü de arttırır. Bunun için daha sonrasında bir LPF(Low-pass-filter) veya BPF(Band-pass-filter) tasarlanabilir.

Tasarlanan filtrelerden sonra artık temiz bir sensör verisine sahip olacağız. Bu işlemlerden sonra peak detection ve SpO2 hesaplaması yapılarak verileri 
PC üzerinde grafiksel olarak gösterilebilir. Ancak ben sinyali okuyup filtrelerden geçirmeme rağmen görselleştirme aşamasında istediğim başarıyı elde edemedim. 

Her ne kadar bu süreçte birçok şey öğrenmiş olsam da, en son aşamada kesin ve doğru diyebileceğim bir görselleştirme elde edemedim. Bunun nedeni büyük 
ihtimalle hem çalışmada faydalandığım kaynağın da belirttiği gibi sensörün kalibrasyon zorlukları olabilir. Projede verileri görşelleştirme ve iyileştirmek için neler yapmam gerektiğine bakacağım fakat şimdilik elimdeki .csv uzantılı veriyi kaydettiğim dosyayı paylaşacağım.

Proje esnasında oldukça fazla faydalandığım kaynak : https://morf.lv/implementing-pulse-oximeter-using-max30100

Max30100 datasheet : https://www.analog.com/media/en/technical-documentation/data-sheets/max30100.pdf

Not : Eğer USB device modunda bilgisayarınız USB yi tanımazsa STM32 ST-LINK Utility ile giriş yapın ve cihazı ST-LINK seçmesinden firmware update yapın.















   
