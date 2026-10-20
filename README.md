# The Air-Gapped Agency — BIL 322 Project 2

C11/POSIX orchestrator; analyst, reporter ve bonus scout. Agent'lar dosya
mailbox üzerinden haberleşir. Uygulama ağ, socket, API, LLM veya `system()`
kullanmaz. Hocanın özgün ödev dosyası değiştirilmemiştir.

## Kurulum ve kullanım

Gerekenler: POSIX ortamı, C derleyicisi, `make`. Testlerde ayrıca POSIX shell,
`awk`, `sed`, `grep`, `tar`, `ps` ve `sleep` kullanılır; Python gerekmez.
**Bütün komutları depo kökünden çalıştırın.**

```sh
make clean && make && make test
./agency
```

Derleme bayrakları: `-std=c11 -Wall -Wextra -Werror`.
`make test` geçici çalışma alanlarında çalışır; kendi veri/görevlerinizi değiştirmez.
`make demo` ayrı geçici alanda derler, gerçek demo çalıştırır ve alanı siler.

```text
agency> tasks
agency> suggest
agency> submit tasks/task_001.task
agency> status task_001
agency> collect task_001
agency> suggest
agency> send analyst reporter task_001 "please brief"
agency> submit tasks/task_002.task
agency> collect task_002
agency> report task_002
agency> submit tasks/task_003.task
agency> collect task_003
agency> exit
```

Shell'e döndükten sonra `cat reports/task_001.briefing` ile briefing görülür.
Aynı çalışma alanında örnek görevleri yeniden başlatmak için önce `make clean && make`
çalıştırın; mevcut ID tekrar submit edilemez. Yeni işler için yeni ID ve output seçin.
Öğrenci adı/numarası [report.md](report.md) içinde teslimden önce doldurulmalıdır.

## Komutlar

| Komut | Davranış |
|---|---|
| `run <program> [args...]` | Doğrudan `execvp`; ID diskten türetilir, stdout/stderr loga gider. |
| `list` | Başlatılan ve kurtarılan süreçler; otomatik WNOHANG reaping. |
| `status <id>` | Kalıcı status dosyasını gösterir, önce reap eder. |
| `wait <id>` | Hedef bitene dek bütün kendi child sonuçlarını toplar. |
| `report <id>` | Raporu gösterir ve `collected: yes` kaydeder. |
| `log <id>` | Tamamlanınca atomik yayımlanan logu gösterir. Çalışırken `.log.tmp` bulunur. |
| `submit <file>` | Görevi doğrular, gerekirse canonical task yayımlar ve agent başlatır. |
| `tasks` | Diskteki görevleri ve başlatılmış işleri birleştirir; başlamayanlar `PENDING`. |
| `collect <id>` | Bekler ve raporu gösterir; yoksa `no report produced`. |
| `send <from> <to> <id> <message>` | Kaynağı/alıcıyı doğrular, numarayı diskten hesaplayıp mesajı yayımlar. |
| `suggest` | Duruma göre öneri verir; komut çalıştırmaz. |
| `help`, `exit` | Yardım; kendi child'larını bekleyip çıkış. |

Tek/çift tırnak ve bir sonraki karakteri kaçıran `\` desteklenir. Expansion,
globbing, pipeline ve otomatik shell yoktur. `run /bin/sh -c 'exit 2'` açıkça
shell çalıştırır. CLI komut hataları stderr'e basılır; interaktif shell devam eder.

## Agent sözleşmesi

```text
./agents/<agent> <task_id> <input> <output>
```

Task'ta `id`, `agent`, `input` zorunlu; output yoksa `data/<id>.out` kullanılır.
Path'ler workspace köküne göre yorumlanır; output'un üst dizini mevcut/yazılabilir
olmalıdır. Mevcut output, başka görevin output'u ve ayrılmış protokol hedefleri
reddedilir. `priority` tamsayı olarak okunur; scheduling bonusu uygulanmaz.

- **analyst:** `id,score` başlıklı iki sütunlu CSV; pozitif integer ID, sonlu
  score, en az bir veri satırı. Quoted CSV desteklenmez. Satır sınırı 8192 byte'ın
  altındadır. LF/CRLF okunur. Demo: 4 satır, sum 320, average 80.00.
- **reporter:** En eski işlenmemiş `to: reporter` mesajını sayısal sıra ile
  seçer. Kaynak raporu briefing'e aktarır. `source_message` metadata'sı ile
  başarılı tüketimi kaydeder. Mesajlar silinmez. Aynı anda tek reporter çalışır.
- **scout:** Case-sensitive, sabit `ERROR` alt dizgisini arar. Eşleşen satırları
  output'a, `rows_processed`/`matching_lines` sayımlarını kendi raporuna yazar.

Agent exit kodları: `0` başarı, `1` sonuç/mesaj/veri satırı yok, `2` bozuk
argüman/veri, `3` I/O hatası. Scout eşleşme bulamazsa boş output ve başarı raporu
ile `1` döner. Her normal exit, kodu ne olursa olsun `state: done` olur.
Exec/fork/waitpid, sinyal veya recovery belirsizliği `state: error` olur.

## Modüller ve kalıcılık

`src/cli.c` tokenizer; `orchestrator.c` komutlar/status sahibi;
`process.c` fork/exec/dup2/reaping; `protocol.c` parser/atomic I/O;
`workspace.c` kilit/ID/recovery; `mailbox.c` submit/tasks/send;
`advisor.c` öneriler. Agent'lar ortak `agents/agent_common.c` kullanır.

Status'ları yalnız orchestrator, her raporu ilgili agent, mesajları orchestrator
yazar. Dosyalar `.tmp` + flush/close + `rename()` ile yayımlanır. Log child FD'sinden
`.log.tmp` içine akar ve reap sonrasında `.log` olarak rename edilir. Aynı anda
tek orchestrator için `fcntl` kilidi vardır; `.agency.lock` silinmez.

Restart status, task ve mesajları diskten yükler. Canlı eski PID izlenebilir;
yeni orchestrator onun parent'ı değildir. Sonuç kaybolmuşsa sahte exit code
atanmaz: `error`, `exit status unavailable after restart`. PID reuse, güç kaybı
dayanıklılığı ve çok dosyalı işlemlerin sınırları [report.md](report.md) içindedir.

`exit` kendi child'larını **bekler**, süresiz programı otomatik öldürmez.
Workspace güvenilen yerel kullanıcıya aittir; rastgele `run` programları bir
sandbox içinde çalışmaz ve ağ erişimleri bu uygulama tarafından engellenmez.
Projenin kendi programları ağ kullanmaz.

## Temizleme

`make clean` kilit alır; etkin orchestrator veya canlı kaydedilmiş süreç varken
reddedilir. Derleme çıktılarını ve ayrılmış `task_*.status`, `task_*.report`,
`task_*.briefing`, `task_*.log`, `data/task_*.out`, `msg_*.txt` ile bunların
temp dosyalarını siler. Orchestrator tarafından oluşturulmuş canonical task'ler
`canonical_created` metadata'sı ile temizlenir. `.gitkeep`, authored task/input
ve ayrılmış runtime adlarını kullanmayan örnekler korunur.
Keyfi haricî/custom output path'leri otomatik silinmez; bunları kendiniz yönetin.
Temizlemeden önce saklamak istediğiniz runtime sonuçlarını farklı adla kopyalayın.

## Doğrulama ve teslim

Gerçek doğrulama: macOS/Apple Clang ve Ubuntu 24.04 arm64/GCC 13; Linux testleri
Docker içinde `--network none --init` ile çalıştırıldı. ASan/UBSan testleri de
geçti. GitHub Actions yapılandırması eklendi; remote olmadığı için CI üzerinde
çalıştırıldığı iddia edilmez.

```sh
make linux-test  # Docker gerektirir; toolchain imajı kurulumunda geliştirme ağı kullanılır
```

- [Gerçek Linux build/test kaydı](docs/validation-linux.txt)
- [Gerçek Linux ASan/UBSan kaydı](docs/validation-sanitizers-linux.txt)
- [Gerçek Linux demo kaydı](docs/demo-linux.txt)
- [T01–T18 kabul matrisi](docs/ACCEPTANCE.md)
- [Tasarım, protokol, AI disclosure ve sınırlamalar](report.md)

Teslim: kaynak/header dosyaları, Makefile, testler, örnek task/veriler, workspace
`.gitkeep` dosyaları, README, report ve doğrulama kayıtları. Binary/runtime
sonuçları Git'e alınmaz. GitHub remote eklenmemiştir; yerel commit geçmişi mevcuttur.
