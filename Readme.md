max30100 sensörü, bir Pulse oksimetre ve kalp atış hızı sensör entegresidir. Oluşturacağınız yazılımlar ile kandaki oksijen miktarını ve kalp atış hızını 
hesaplayabilirsiniz.

max30100 sensörü onaylı bir cihaz değildir, bu sebeple hiçbir koşulda gerçek bir hastalığı test etmek için kullanılamaz. Kendi akıllı saatini, 
bilekliğini, taşınabilir sağlık ürününü ya da sadece işlemcilerle çalışma yapmak isteyen herkes için kullanılabilir. Max30100 sensörünün, I2C protokolü ile
oldukça basit bir veri okuma yapısına sahip olması, sensörden doğru verinin hemen alınabileceği fikrini ortaya çıkarmamalı.(Neden bu sensörle uğraşmak istediğimin 
nedeni de bu) Sensörden alınan ham veriyi birçok aşamadan geçirmeli ve kalibrasyon yeteneği zayıf olan sensörün, doğru veriyi alması adına biraz uğraşmalısınız.

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















   
