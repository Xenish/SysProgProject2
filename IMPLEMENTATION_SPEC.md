# BIL 322 — Project 2: The Air-Gapped Agency
## Implementation Specification / Codex için kesin teknik sözleşme

**Sürüm:** 1.0 — 21 Ekim 2026  
**Durum:** Planlama tamamlandı; uygulama kodu henüz yazılmadı.  
**Kaynak:** `project2-the-air-gapped-agency.md` (öğretim elemanı tarafından verilen ödev).  
**Hedef:** Önce zorunlu 100 puanlık gereksinimler, ardından üçüncü agent (+5).  
**İlke:** Buradaki kararlar hocanın açık gereksinimlerinin *yerini almaz*; hoca belirsiz bıraktığında bu belge uygulanacak tasarımı tarif eder.

---

## 1. Kapsam ve değişmez koşullar

1. Ana program (`agency`) ve agent'lar **C11 + POSIX** olacak; derleyici uyarıları `-Wall -Wextra -Werror` ile sıfır olacak. İsteğe bağlı `-pedantic` eklenebilir; POSIX tanımları için `_POSIX_C_SOURCE=200809L` kullanılacak.
2. Ağ iletişimi, sockets, HTTP, curl, uzak API, LLM çağrısı **uygulamada yasak**. Git/GitHub sadece geliştirme ve teslim aracı; agent iletişim kanalı değil.
3. CLI komutları tam olacak: `run`, `list`, `status`, `wait`, `report`, `log`, `exit`, `submit`, `tasks`, `collect`, `send`, `suggest`. Yardımcı `help` komutu eklenebilir.
4. Paylaşılan dosyaların yayımlanması `final.tmp` + `rename(final.tmp, final)` yoluyla atomik yapılacak; hata kontrolü ve `fclose`/flush başarısı doğrulanacak. Aynı final dosyaya eşzamanlı iki yazar olmayacak.
5. Durum dosyasını yalnızca orchestrator; standart raporu yalnızca ilgili agent; mesaj dosyasını yalnızca orchestrator yazacak. Agent'ın stdout/stderr'i `dup2` ile ilgili loga yönlendirilecek.
6. `done` = process normal çıktı (exit code 0 dışındaki değerler dahil); `error` = altyapı başarısızlığı (`fork`/`exec`/`waitpid`, sinyal ile ölüm, durum kurtarmada sonucun bilinmemesi). Agent mantıksal başarısızlığı `done` + sıfır olmayan `exit_code` ile ifade edilir.
7. Takip edilen çocuk process'ler `waitpid(..., WNOHANG)` ile otomatik reap edilecek. Etkileşimli promptta beklerken de reaping yapılacak; zombie kalıcı olarak tutulmayacak.
8. `tasks/`, `status/`, `reports/`, `messages/`, `logs/`, `data/` Git'te `.gitkeep` ile bulunacak; `make clean` bu placeholder'ları ve elde hazırlanmış giriş/örnek task dosyalarını silmeyecek.
9. Crash/restart sonrasında RAM'de olan değil, diskte bulunan status/task/message/report dosyaları esas alınacak.
10. Üçüncü agent `scout` (+5) temel uygulama bitince eklenecek. GUI, JSON, timeout/retry, priority scheduling ve yerel LLM varsayılan kapsamın dışında.

## 2. Platform ve repo

- Repo: `air-gapped-agency` (isim gerekirse GitHub üzerinde değiştirilebilir).
- Yerel geliştirme: macOS + Codex CLI. Kanonik kabul testi: Ubuntu/Linux. GitHub Actions ile `make clean && make && make test` Linux üzerinde çalıştırılabilir.
- Derleme: `make`; temizleme: `make clean`; test: `make test`; demo: `make demo` (isteğe bağlı otomasyon). Repo kökünden `./agency` ile başlatılır.
- C ve POSIX dışında bağımlılık yok. Test otomasyonunda POSIX shell yardımcı scriptleri kullanılabilir.
- `gitignore`: derleme çıktısı, binary, object, geçici/üretilmiş status-report-log-message ve geçici veri çıktıları. Takip edilecekler: kaynak kod, Makefile, testler, başlangıç görevleri, örnek veri, `.gitkeep`, README.md, report.md.

### Kaynak yapısı (sorumlulukların tek sahibi olacak)

```text
air-gapped-agency/
├── src/
│   ├── main.c                 # boot, workspace kontrolü, event loop
│   ├── cli.c                  # tokenization, arg doğrulama, dispatch
│   ├── orchestrator.c         # task tablosu, komut davranışları
│   ├── process.c              # fork/execvp/waitpid/SIGCHLD/dup2
│   ├── workspace.c            # dizin gezme, path oluşturma, recovery
│   ├── protocol.c             # key:value parser, atomic writer
│   ├── mailbox.c              # submit/collect/send ve mesaj sıra sayısı
│   └── advisor.c              # kural tabanlı suggest
├── include/
│   ├── agency.h               # shared types, limits, state constants
│   ├── cli.h
│   ├── orchestrator.h
│   ├── process.h
│   ├── workspace.h
│   ├── protocol.h
│   ├── mailbox.h
│   └── advisor.h
├── agents/
│   ├── analyst.c              # score CSV analizi
│   ├── reporter.c             # mailbox -> briefing
│   ├── scout.c                # ERROR satırı bulma (bonus)
│   ├── agent_common.c         # ortak agent I/O yardımcıları
│   └── agent_common.h
├── tasks/
│   ├── .gitkeep
│   ├── task_001.task          # elde hazırlanmış, izlenen analyst demo görevi
│   ├── task_002.task          # elde hazırlanmış, izlenen reporter demo görevi
│   └── task_003.task          # isteğe bağlı scout demo görevi
├── status/.gitkeep
├── reports/.gitkeep
├── messages/.gitkeep
├── logs/.gitkeep
├── data/
│   ├── .gitkeep
│   ├── survey.csv             # izlenen giriş örneği
│   ├── report_request.txt     # izlenen reporter input örneği
│   └── events.txt             # izlenen scout input örneği
├── tests/
│   ├── run_tests.sh
│   └── ...                     # hata senaryoları / fixture
├── .github/workflows/ci.yml   # isteğe bağlı Linux CI
├── Makefile
├── .gitignore
├── README.md
├── report.md
└── IMPLEMENTATION_SPEC.md
```

### Modüller arası sınırlar

- `cli`: Kullanıcı girdisini güvenli biçimde token'lara ayırır; dosyalara/child process'lere doğrudan dokunmaz.
- `orchestrator`: Komut çağrıları ve RAM'deki geçici task kaydı; status sahibi olarak dosya güncellemelerini başlatır.
- `process`: Process oluşturma, stdout/stderr loga yönlendirme, exit status ayrıştırma, zombie reaping. Agent raporu **yazmaz**.
- `protocol`: Genel `key: value` okuma/doğrulama ve atomik yazma; iş kuralı içermez.
- `workspace`: Klasörler, ID tarama, yeniden açılışta diskteki durumu yeniden kurma.
- `mailbox`: `.task` okuma/submit doğrulama, `send` sıralaması, collect davranışı.
- `advisor`: İş durumunu okur ve öneri üretir; task başlatmaz.
- `agent_common`: Agent argüman sözleşmesi + her agent'ın kendi raporunu atomik yayımlaması.

## 3. Sabit protokol ve dosya kuralları

### 3.1 Kimlik ve dosya adı

- Task ID: `task_` + en az üç basamak (ör. `task_001`; 999 sonrası `task_1000`). ID bir string'dir.
- `status/<id>.status`, `reports/<id>.report`, `logs/<id>.log` her durumda ID'den türetilir.
- Mesaj: `messages/msg_<seq>.<from>.<to>.txt`. Sıra sayısı en az 3 basamaklıdır.
- Agent adları güvenli karakter kümesiyle sınırlıdır: `[A-Za-z0-9_-]+` (path traversal kapalı).
- Tüm path'ler repo/workspace köküne göre yorumlanır; örnekler `data/survey.csv` gibi relative path'tir. Demo/proje için repo kökünden çalıştırma zorunlu, README'de yazılı.
- Bir dosyada anahtarlar tek satırlık `key: value` formatında; `:` karakteri değer içinde kullanılabilir (ilk `:` ayırıcı). Bilinmeyen anahtarlar yok sayılabilir, zorunlu anahtar tekrarları reddedilir. Satır/alan uzunluğu kontrollü ve hata mesajı açıklayıcı olacak.

### 3.2 Task

```text
id: task_001
agent: analyst
input: data/survey.csv
output: data/task_001.out
priority: 1
```

- Zorunlu: `id`, `agent`, `input`. `output` yoksa **`data/<id>.out`** varsayılır. `priority` parse edilir fakat bonus priority scheduling uygulanmayacaktır; FIFO/shell çağrı sırası korunur.
- `submit`: Task dosyasını okur; geçersiz ID, eksik alan, okunmayan input, bulunmayan/çalıştırılamayan `./agents/<agent>` veya geçersiz output path için **fork etmeden** hata döndürür.
- `tasks/task_001.task` dosyasının bulunması normaldir (bu input!). **Duplicate** demek o task'ın `status/task_001.status` ile daha önce başlatılmış olması veya çalışma tablosunda etkin olmasıdır. Yani mevcut `.task` dosyasını ilk kez submit etmek duplicate değildir.
- Başka konumdaki task dosyası kabul edilirse başarılı submit öncesi kanonik `tasks/<id>.task` dosyasına atomik kopyalanır. Bu path zaten farklı bir task ile doluysa hata; canonical dosyanın kendisi kullanılabilir.
- Bir ID bitmiş olsa dahi yeniden submit edilemez; yeni ID gereklidir. Advisor bunu doğru ifade eder.

### 3.3 Agent launch contract (değiştirilemez)

```text
./agents/<agent> <task_id> <input> <output>
```

`execvp` argv içeriği: `{ "./agents/analyst", "task_001", "data/survey.csv", "data/task_001.out", NULL }`. Agent `argc == 4` kontrolü yapar.

### 3.4 Status

```text
id: task_001
agent: analyst
pid: 12345
state: running
collected: no
```

Başarılı *process* bitişi:

```text
id: task_001
agent: analyst
pid: 12345
state: done
exit_code: 2
report: reports/task_001.report
collected: no
```

Altyapı hatası:

```text
id: task_001
agent: analyst
pid: 12345
state: error
message: child terminated by signal 15
collected: no
```

- `collected: yes|no` ödevin zorunlu alanı değil, **bizim eklediğimiz, orchestrator'ın yazdığı metadata**. `report` veya `collect` başarılı rapor gösterince `yes` yapılır, `suggest` bunu kullanır.
- `done` raporun mutlaka oluştuğu anlamına gelmez. Normal çıkan fakat rapor yazmayan agent da `done`; `collect` `no report produced` bildirir.
- `execvp` başarısızlığını genel `exit(127)` ile karıştırmamak için parent/child arasında yalnızca exec hatası bildirimi amaçlı `FD_CLOEXEC` pipe kullanılabilir (ödev buna izin verir). Daha basit bir çözüm uygulanırsa `127` yalnızca exec failure için ayrılmalı; diğer normal çıkışlar doğru sınıflandırılmalıdır. **Tercih: hata pipe'ı.**
- `fork` hatasında PID bulunmayabilir; `error` durumunda `pid` opsiyoneldir.

### 3.5 Raporlar ve agent output

- Her agent yalnızca kendi ID'sine ait `reports/<id>.report` dosyasını **atomik** üretir.
- Ayrıca task'taki `<output>` hedefini **atomik** üretir. Bu iki dosya aynı şey olmak zorunda değildir.
- Analyst başarılı rapor örneği:

```text
agent: analyst
input: data/survey.csv
rows_processed: 4
sum: 320
average: 80.00
status: success
```

- Agent mantıksal hatası: anlamlı hata raporu yazmaya çalışır (`status: failure`, `message: ...`) ve `exit_code: 2` gibi sıfır olmayan normal çıkış kodu ile biter. Dolayısıyla orchestrator `state: done` üretir.
- Exit sözleşmesi (agent'lar): `0` başarı; `1` makul sonuç yok/beklenen iş sonucu; `2` geçersiz veri veya argüman; `3` dosya/rapor I/O hatası. Sinyal ile ölüm ve exec hatası process katmanındadır.

### 3.6 Mesaj

```text
from: analyst
to: reporter
task: task_001
message: please brief
```

- `send` komutunda ilk dört argüman `from`, `to`, `task_id`, `message` olacak. Mesaj birden fazla kelime olabilir; terminalde tırnak desteklenir. Newline ve kontrol karakterleri reddedilir.
- `from`, kaynak görevin `agent` alanıyla aynı olmalı; `to` bilinen agent olmalı; kaynak görev bulunmalı. Demoda önce analyst raporu üretilmiş olmalı. `send` çağrısı `msg_001.analyst.reporter.txt` biçiminde atomik dosya yayımlar.
- Sıra numarası **her `send` öncesinde messages klasöründeki tüm `msg_*.txt` dosyalarından max+1 ile** hesaplanır. Bellek içi sayaç tek kaynak olamaz. Mesajlar silinmeyecek; böylece eski numaralar asla yeniden kullanılmaz.
- Reporter işlenen mesajları tekrar seçmemek için mevcut başarılı reporter raporlarındaki `source_message: msg_001.analyst.reporter.txt` alanına bakar. En eski işlenmemiş `to: reporter` mesajını seçer. Bu metadata raporun tek yazarı olan reporter tarafından yazılır; ayrı bir `ack` dosyasına ihtiyaç yoktur.
- Aynı anda birden fazla reporter task'i başlatmak reddedilir (aynı mesajı iki kere tüketme yarışını önlemek için). Bu, sadece uyguladığımız basit demo protokolünün bilinen sınırlamasıdır.

### 3.7 Analyst -> Reporter kesin demo

Başlangıç dosyaları:

`tasks/task_001.task`:

```text
id: task_001
agent: analyst
input: data/survey.csv
output: data/task_001.out
priority: 1
```

`tasks/task_002.task`:

```text
id: task_002
agent: reporter
input: data/report_request.txt
output: reports/task_001.briefing
priority: 1
```

`data/survey.csv`:

```text
id,score
1,70
2,80
3,90
4,80
```

`data/report_request.txt`: `Produce a briefing from the oldest unread analyst message.`

Demo komutları:

```text
./agency
agency> submit tasks/task_001.task
agency> wait task_001
agency> collect task_001
agency> send analyst reporter task_001 "please brief"
agency> submit tasks/task_002.task
agency> collect task_002
agency> report task_002
agency> suggest
agency> exit
```

**ÖNEMLİ:** Reporter ikinci, ayrı task olan `task_002` ile çalışır (duplicate ID yasağı). Analyst'in raporunu `reports/task_001.report` üzerinden okur, `reports/task_001.briefing` üretir ve **kendi** agent raporunu `reports/task_002.report` olarak oluşturur. Bu, ödevin demo örneği ile genel `<id>.report` dosya sözleşmesini çakışmadan birlikte sağlar.

## 4. Agent'ların kesin işleri

- **Analyst:** `data/survey.csv` gibi `id,score` başlıklı iki sütunlu basit CSV girişi okur. Başlığı doğrular; her veri satırında pozitif integer `id` ve sonlu sayısal `score` bekler; eksik/bozuk satırı sessiz geçmez. `rows_processed`, `sum`, `average` hesaplar. `<output>` dosyasına insan tarafından okunur özet; `reports/<own-id>.report` dosyasına key:value rapor yazar.
- **Reporter:** `messages/` klasöründe hedefi `reporter` olan en eski **işlenmemiş** mesajı tarar; mesajın `task` alanından `reports/<source-task>.report` dosyasını açar; içerikteki önemli metrikleri özetleyerek kendi `<output>` hedefinde briefing oluşturur; ayrıca `reports/<own-id>.report` içinde `source_task`, `source_message`, `briefing`, `status` yazar. Önceden işlenen mesajı tekrar işleme almaz. İlgili mesaj veya kaynak rapor yoksa belgelenmiş sıfır olmayan exit code ve hata raporu verir.
- **Scout (bonus):** Düz metin `<input>` dosyasında `ERROR` geçen satırları (case-sensitive, basit `strstr`) tespit eder. Eşleşmeleri `<output>` dosyasına; toplam okunan/eşleşen satırları ve `status` alanını `reports/<own-id>.report` dosyasına yazar. Keyword sabittir ve README'de anlatılır; görev sözleşmesi üç pozisyonel argümandan genişletilmez.
- **Yapay zekâ / ML** agent'larda yoktur, çünkü hoca açıkça bunu istemiyor.

## 5. CLI komut davranışları

| Komut | Kararlaştırılmış davranış |
|---|---|
| `run <program> [args...]` | Harici process çalıştırır; çakışmasız yeni ID seçer, stdout/stderr loga gider; `status/` dosyası oluşturur. `run` için `.task` dosyası veya agent raporu zorunlu değildir. |
| `list` | `waitpid(WNOHANG)` ile canlı çocukları reap eder; başlatılmış işler için ID, PID, state gösterir. |
| `status <id>` | Önce reap, sonra diskteki status bilgisini okur; istenirse henüz submit edilmemiş `.task` için `pending` gösterir. |
| `wait <id>` | Bekleyen kendi çocuklarını `waitpid` ile sonuçlanana dek toplar; daha önce bitmişse hemen döner. Recovered detached PID için yalnızca liveness izlenebilir; gerçek exit code geri kazanılamaz. |
| `report <id>` | Raporu gösterir; başarılı gösterimde `collected: yes` yazar. Dosya yoksa açıklayıcı mesaj basar. |
| `log <id>` | `logs/<id>.log` içeriğini gösterir. Yoksa açıklayıcı mesaj. |
| `submit <file>` | Parse/validate; canonical workspace task kaydı; `./agents/<agent>` process'ini sabit argv ile fork/exec; ID duplicate kontrolleri. |
| `tasks` | `tasks/*.task` + `status/*.status` birleştirilir; henüz başlamamışlar `PENDING`, başlayanlar diskteki stateleriyle gösterilir. |
| `collect <id>` | Gerekiyorsa `wait`; sonra rapor; yoksa `no report produced`. Başarılı rapor okuması `collected: yes`. |
| `send <from> <to> <id> <message>` | Parametreleri doğrular, mesaj sıra numarasını diskten bulur, atomik dosya yazar. |
| `suggest` | Workspace durumuna göre yalnızca öneri verir; komut çalıştırmaz. |
| `exit` | Canlı **kendi** child process'lerini güvenli şekilde reap/bitmesini bekleyerek çıkar; bounded olmayan program süresini otomatik öldürmez (README'de belirtilir). |
| `help` | Kullanılabilen komutları gösterir (yardımcı). |

**Komut ayrıştırma:** Tırnak içinde boşluk (`"please brief"`) ve basit escape'ler desteklenecek. Maksimum satır/argüman sayısı sınırlı; taşmalar hata olacak. `run` /bin/sh ile otomatik shell execute etmez, doğrudan `execvp` kullanır; kullanıcının açıkça `run /bin/sh -c "..."` vermesi mümkündür.

**Yeni ID üretimi:** `tasks/*.task` ve `status/*.status` dosyalarındaki en büyük task numarasını tara, +1. Örnek task_001/task_002 repo içinde mevcut olduğundan ilk `run` genellikle task_003 veya scout demo task_003 mevcutsa task_004 olur. Hard-coded ilk `run` ID'sine güvenen test yazılmayacak.

## 6. Process lifecycle ve güvenlik

1. `submit`/`run` doğrulama; ID rezervasyonu, log hazırlanması, child başlatılması.
2. Parent `fork`; child `dup2` ile stdout/stderr'i log dosyasına bağlar, `execvp` yapar. `execvp` hatasında sadece child `_exit(127)`; hata parent'a POSIX parent-child pipe ile iletilir.
3. Parent `status: running` atomik yazar; iş bitince `waitpid` sonucu üzerinden `done`/`error` atomik yazar.
4. `SIGCHLD` sinyal handler'ı sadece `volatile sig_atomic_t` bayrağı ayarlar; **dosya I/O, malloc, printf veya waitpid handler içinde yok**. Main event loop, bloklayan giriş beklemesinden sinyalle uyanır (`SA_RESTART` kullanılmaz), güvenli bağlamda `waitpid(-1, ..., WNOHANG)` çalıştırır. `list` ve `status` da açıkça bu reaper'ı çağırır.
5. `wait` tek PID üzerinde beklerken başka child'ların da çıkabileceğini dikkate alır; `waitpid(-1, ...)` sonuçlarını ilgili task'a dağıtarak tüm çocukları reap eder. `EINTR` ile doğru şekilde başa çıkar.
6. Durum yazımı `write(temp) -> flush -> close -> rename` şeklindedir; temp/final aynı dizindedir. Hatalı publish final dosyayı bozmamalıdır.
7. Aynı workspace'e **aynı anda iki orchestrator** başlamasına izin verilmez. `fcntl` advisory lock (`.agency.lock`) kullanılır. Restart'tan sonra kilit işletim sistemi tarafından serbest kalmış olacaktır.
8. Orchestrator'ın açılışında RAM tablosu diskten yeniden kurulur. `running` görülen PID için `kill(pid,0)` ile liveness sorgulanır. Eski process artık bu orchestrator'ın çocuğu değilse `waitpid` exit code'u geri getiremez; sonuç belirsizse **asla sahte `done:0` yazılmaz**. Ölmüş/durum bilgisi kayıp process için `error: exit status unavailable after restart` yazılır. Canlı orphan varsa izleme yapılır, ancak ilişki ve PID reuse sınırlamaları raporda açıklanır.
9. Exit code 0 olmayan normal çıkışlar (örneğin 2) `state: done` olmalıdır. `execvp` hatası ve sinyalle ölüm `state: error` olmalıdır. Bu ayrım zorunlu testtir.

## 7. Advisor öncelik sırası

`suggest` yalnızca şu state + disk kanıtlarını kullanır:

1. `error` task veya `done` ama sıfır olmayan exit code varsa: `log <id>` ve düzeltildikten sonra **yeni ID** ile task oluştur önerisi.
2. `running` task varsa: `tasks` / `status <id>` önerisi.
3. `done` ve `collected: no` ve rapor var ise: `collect <id>`.
4. Başarılı analyst raporu mevcut, bu kaynak için briefing ve ona yönelik mesaj yoksa: `send analyst reporter <id> "please brief"`.
5. Reporter'a mesaj var ama briefing yoksa: (henüz submit edilmemiş geçerli reporter task varsa) `submit tasks/task_002.task`.
6. Hiç başlatılmış görev yoksa: `submit tasks/task_001.task`.
7. Aksi halde: `No pending suggestions.`

Advisor **otomatik iş yapmaz**. Kural sırası, `report.md` içinde açıklanır.

## 8. Test ve acceptance matrix

Testler ayrı, geçici çalışma dizininde veya kontrollü fixture'larla çalışmalı; kullanıcının elle hazırladığı dosyalara zarar vermemeli.

| ID | Beklenen davranış |
|---|---|
| T01 | `make clean && make` sıfır uyarı/sıfır hata. |
| T02 | `run /bin/echo hello` log dosyasında `hello`; `list/status/wait/log` çalışır. |
| T03 | Kısa süreli child, açık CLI'da ayrıca `wait` yazmadan reap edilir; zombie kalmaz. |
| T04 | Doğru analyst task'i `done, exit_code:0`, geçerli rapor ve output üretir. |
| T05 | Tekrarlanan `submit task_001` açık duplicate hatası üretir, mevcut veriyi ezmez. |
| T06 | Missing `id` / `agent` / `input`, executable yok veya input okunamaz: fork öncesi hata. |
| T07 | Bozuk CSV -> agent normal çıkışı nonzero; orchestrator `done` + `exit_code:2` yazar (error değil). |
| T08 | Sinyalle ölen child -> `error`; exec başarısızlığı -> `error`. |
| T09 | Rapor yazmayan fixture normal çıkarsa `collect` çökmeden `no report produced` der. |
| T10 | Mesaj `msg_001.analyst.reporter.txt` formatında; sırası diskten max +1. |
| T11 | Reporter task_002, analyst report task_001'i okur; briefing + kendi raporunu yazar; `source_message` kaydeder. |
| T12 | Farklı durumda en az iki farklı `suggest` çıktısı; hiçbir task'ı otomatik çalıştırmaz. |
| T13 | Orchestrator kapatılıp açılınca eski ID/durum/mesajlar görünür, sayaç yeniden kullanılmaz. |
| T14 | Atomic writer kaynak kodunda `.tmp` + `rename`; başarısız yazma final dosyayı parçalı bırakmaz. |
| T15 | `make clean` oluşturulan status/log/report/message/demo output'ları kaldırır, `.gitkeep` ve tracked input dosyalarını korur. |
| T16 | Scout `ERROR` eşleşmeleri için ayrı sonuç/rapor üretir (+5 puan bonus). |
| T17 | `make test` Linux/macOS'ta başarılı; Linux `ps` kontrolünde projeden kalma zombie yok. |
| T18 | `report.md` hoca tarafından istenen tüm başlıkları, gerçek demo çıktısını, bilinen sınırlamaları, AI disclosure'ı içerir. |

Not: `ps aux | grep defunct` başka programlara ait process'leri de gösterebilir; **bizim yarattığımız child PID'lerini** bekleme ve reaping kanıtlarıyla kontrol et, demo sırasında hocanın istediği komutu da çalıştır.

## 9. Geliştirme safhaları ve commit noktaları

- **P0 Plan & repo:** Klasörler, `.gitignore`, `IMPLEMENTATION_SPEC.md`, README taslağı, Makefile iskeleti, `.gitkeep`, sample data/task. Commit: `chore: initialize agency project structure`.
- **P1 Orchestrator:** CLI, `run/list/status/wait/report/log/exit`, reaping, atomik status. `make`, başlangıç testleri. Commit: `feat: implement process orchestrator shell`.
- **P2 Mailbox:** Parser, submit/tasks/collect, input validation, persistent task table, restart. Commit: `feat: add file mailbox task lifecycle`.
- **P3 Agents:** Analyst, reporter (mesaj entegrasyonu sonra), agent report contract, fixtures. Commit: `feat: implement analyst and reporter agents`.
- **P4 Messaging:** send, diskten message ID, reporter mailbox tüketimi, analyst->reporter briefing end-to-end. Commit: `feat: implement agent message exchange`.
- **P5 Advisor:** rule-based suggest, testler. Commit: `feat: add state based advisor`.
- **P6 Bonus:** scout. Commit: `feat: add scout agent`.
- **P7 Quality & docs:** Fault injection, restart, make clean, README, report, CI, final run. Commit: `test: verify lifecycle and end to end demo`, ardından `docs: complete project report and usage guide`.

Her faz sonrası: `make clean && make && make test` → `git diff` → ilgili dosyalarda kod incelemesi → commit. Kod çalışmıyorsa fazı bitmiş sayma.

## 10. Teslim / rapor / demo

`report.md` zorunlu: öğrenci adı + numarası placeholder (gerçeği kullanıcı ekleyecek), protokol örnekleri, dosya ownership, `submit`->`execvp` argv, eksik alan stratejisi, recovery örnekleri ve sınırlama, gerçek terminal demo transcript'i, bilinen bug/limitasyon, AI disclosure. AI disclosure gerçek kullanılan araç ve hangi parçalarda kullanıldığı; gerçekte çalıştırılmamış testler çalışmış gibi yazılmayacak.

Demo sırası: `make clean && make` → `./agency` → pending task `tasks` → analyst `submit` → anlık `status` ve `status/` göster → `collect` → `send` → reporter `submit` → `collect task_002`, `cat reports/task_001.briefing` → farklı state'lerde `suggest` → `ps aux | grep defunct` → status atomic writer'ı kodda göster. `exit` sonrası dosyalar kalır; restart ile görünür.

## 11. Kapsam dışı ve önemli sınırlamalar

- Bonus priority **uygulanmıyor**, `priority` yalnızca format uyumluluğu için okunuyor.
- GUI/JSON/LLM yok. Üçüncü agent dışında bonus önceliği yok.
- Crash restart sonrası eski child'ın gerçek exit code'u `waitpid` ile elde edilemez. PID tekrar kullanımı `kill(pid,0)` için potansiyel belirsizliktir; raporda saklanmayacak.
- Genel amaçlı tam RFC-uyumlu CSV parser gerekmez; demo için açıkça belgelenen iki sütunlu CSV yeterli.
- Parser tam bir Unix shell değildir; command expansion, globbing, pipelines yok.
- Mesajlar klasörde kalır; reporter işlenmiş mesajları kendi rapor metadata'sından tanır; birden çok reporter eşzamanlı başlatılmaz.
- `make clean` kullanıcı tarafından keyfi bir dış path'e yazdırılmış dosyaları güvenlik nedeniyle silmeye zorlanmaz; bilinen/generated workspace dosyalarını temizler.

## 12. Codex kalite sözleşmesi

1. Ödev dosyası ve bu spesifikasyon okunmadan kod yazma.
2. Tek faz dışına izinsiz scope genişletme; gelecekteki fazlar için stub veya TODO bırakılabilir ama mevcut fazın testsiz tamamlandığını söyleme.
3. Devasa `main.c` oluşturma; yukarıdaki modüllere uy.
4. Tüm public fonksiyonlarda başarı/hata yolu; return değeri kontrolü; bellek kaynaklarını kapat; **C/POSIX best practice**.
5. Shell injection yok; `system()` kullanma. Agent'ları `fork` + `execvp` ile başlat.
6. Core'da network ve ek paket yok. Bilinmeyen davranışı otomatik icat etme; spec veya ödevdeki en yakın kurala uy; çözülmeyen önemli çelişkiyi kod üretmeden kullanıcıya bildir.
7. Dönemsel `make clean && make && make test` çıktısını göster; uyarı ve başarısız testleri saklama.
8. Her fazda commit'ten önce `git status` ve `git diff --check`; kullanıcının gerçek isim/numara detaylarını uydurma.
9. Genel `README.md` ile kodu açıklayıcı `report.md` birbirinden ayrı tutulacak.
10. Kaynak projedeki politika/ödev açıklaması değiştirilmez.
