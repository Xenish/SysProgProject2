# Codex Execution Guide — Air-Gapped Agency

## 0. İlk mesaj (Codex çalışma sözleşmesi)

> Bu repoda BIL 322 Project 2: The Air-Gapped Agency geliştireceğiz. Önce `project2-the-air-gapped-agency.md` ve `IMPLEMENTATION_SPEC.md` dosyalarını tam oku. Ödevin zorunlu gereksinimlerinin önceliği var; diğer tüm belirsiz kararlarda IMPLEMENTATION_SPEC bağlayıcıdır. C11/POSIX, Makefile, -Wall -Wextra -Werror, modüler mimari, atomik tmp+rename, doğru process reaping ve durum semantiği zorunlu. Sockets/HTTP/API/LLM yok. Her faz sonunda derle/test et, sadece test edilen şeyleri başarılı say. İlgili faz dışında yeni özellik ekleme. Commit geçmişini küçük ve anlamlı tut; master/main dalına doğrudan force-push yapma. Önce iki dosya arasındaki varsa gerçek çelişkileri raporla ve dosya ağacını kısaca özetle; henüz kod yazma.

## P0 — Repo kurulum, yalnızca iskelet

> Faz P0'ı uygula: IMPLEMENTATION_SPEC bölüm 2 ve 9'daki dizinleri, `.gitkeep` dosyalarını, Makefile hedeflerini, .gitignore, izlenen demo task ve veri dosyalarını, kısa README'yi oluştur. Kodun henüz ana algoritmasını yazma. Repo root'tan make hedefleri tutarlı olsun. Git repository'i mevcut değilse initialize et; remote bilgisi kullanıcı tarafından verilmemişse uydurma. Oluşturulan dosyaları kontrol et ve `chore: initialize agency project structure` commiti hazırla.

## P1 — Shell ve process lifecycle

> Faz P1'i uygula: `run`, `list`, `status`, `wait`, `report`, `log`, `exit`, `help`; modüler CLI, process tracking, SIGCHLD reaping, `fork`/`execvp`, `dup2`, status ownership ve atomic writer. Özellikle normal nonzero exit -> `done`, exec/signal/fork -> `error` ayrımını yap. `list` ve `status` WNOHANG kullanmalı. `run` task ID'sini diskten çakışmasız seçmeli. P1 testlerini ekle. `make clean && make && make test` ile çalıştır. Çıktıyı göster; sorun varsa düzelt; ardından `feat: implement process orchestrator shell` commiti oluştur.

## P2 — Task files / mailbox

> Faz P2'yi uygula: `submit`, `tasks`, `collect`, canonical path'ler, `id/agent/input` zorunluluğu, eksik `output` için `data/<id>.out`, duplicate kontrolü, exec argv sözleşmesi, status/workspace persistence ve recovery. `.task` dosyasının varlığı tek başına duplicate sayılmaz, status bulunması daha önce submit edildiğini gösterir. Preflight invalid task varsa fork etme. Otomatik cleanup değil, düzgün error. P2 testlerini ekle, geçince `feat: add file mailbox task lifecycle` commitini oluştur.

## P3 — Gerçek agent'lar

> Faz P3'ü uygula: `analyst` ve `reporter` ayrı executable C programları olsun. Agent argv sözleşmesine ve own-ID report ownership'e uy. Analyst iki sütunlu demo CSV'yi işlemeli, ölçümleri üretmeli. Reporter mesaj entegrasyonu Phase 4'te tamamlanacağından bu aşamada çalışma modülü, raportlama ve giriş/çıkış yardımcılarını hazırla; eksik mesaj durumunu düzgün hata raporuyla ele al. Output ve report dosyaları atomik yayımlansın. T03-T07 benzeri testleri geçir, `feat: implement analyst and reporter agents` commiti oluştur.

## P4 — Agent-to-agent mesajlaşma

> Faz P4'ü uygula: `send` komutu, mesaj kimliğinin diskten max+1 hesaplanması, atomic messages, reporter'ın `to: reporter` olan en eski işlenmemiş mesajı seçmesi, kaynak raporu okuyup kendi output briefing ve own-ID raporu yazması. `task_001` analyst ile `task_002` reporter farklı task ID'leridir; briefing `reports/task_001.briefing`, reporter raporu `reports/task_002.report` olmalı. Mesajları silme; işlenenleri reporter rapor `source_message` metadata'sı üzerinden ayıkla. E2E testi çalıştır, `feat: implement agent message exchange` commiti oluştur.

## P5 — Rule based advisor

> Faz P5'i uygula: `suggest` komutu `IMPLEMENTATION_SPEC` bölüm 7'deki sıralamaya uyacak. Hiçbir komutu kendisi çalıştırmayacak. Workspace gerçek durumuna göre en az iki ayrı test senaryosu yaz. Test geçince `feat: add state based advisor` commiti oluştur.

## P6 — Bonus scout

> Faz P6'yı uygula: üçüncü bir gerçek C agent olan `scout`u ekle. Argüman düzeni aynı. Düz metin input'ta case-sensitive `ERROR` içeren satırları kendi output'una, sayımları own-ID report'una atomik yazsın. Sample fixture ve test ekle. Hoca zorunlu komutlarını bozma. `feat: add scout agent` commiti oluştur.

## P7 — Son kalite kapısı ve rapor

> Faz P7'yi uygula: `IMPLEMENTATION_SPEC` bölüm 8'deki T01–T18 test ve acceptance kriterlerini tek tek gözden geçir. `make clean && make && make test` çalıştır, warning/fail olmadığını doğrula. Hocanın demo komutlarını gerçek çalıştır ve çıktıları kullanarak README ve `report.md` doldur. `report.md`: protokol/ownership/argv, eksik alan kararları, crash recovery ve gerçek sınırlamaları, gerçek demo çıktısı, AI kullanım açıklaması. Kullanıcının adı/numarasını uydurma; placeholder bırak. Kodda unused/unsafe parçalar varsa düzelt. `git diff --check`, `git status`, test çıktılarını özetle. Sonra `test: verify lifecycle and end to end demo` ile `docs: complete project report and usage guide` commitlerini mantıklı dosya kapsamlarıyla oluştur.

## Codex'e her faz sonunda sorulacak kontrol

> Sadece bu faz için: (1) değişen dosyalar, (2) verilen tasarım kararlarının tam karşılandığı yerler, (3) çalıştırılan komutların gerçek stdout/stderr sonuç özeti, (4) geçen/kalan testler, (5) bulduğun riskler, (6) commit hash. Henüz çözülmemiş hata varsa sonraki faza geçme.

## Git / GitHub planı

- Tek kişi: `main` + `feature/p1-orchestrator`, `feature/p2-mailbox`, ... biçiminde kısa branch'ler; her faz merge / PR opsiyonel ama GitHub'daki anlamlı commit geçmişi zorunlu. Küçük projede tek kişi için sürekli PR zorunlu değil.
- İki kişi: `main` korumalı, faz bazlı branch/PR, mümkünse reviewer.
- `git add .` körlemesine yapılmamalı; `git status`, `git diff --cached` kontrol edilmeli, generated log/report/runtime dosyaları commitlenmemeli.
- GitHub repo oluşturma ve `git remote add origin` adımı gerçek kullanıcı hesabı/repo URL'si belli olduğunda yapılmalı, Codex'in uyduracağı bir URL kullanılmamalı.
- GitHub üzerinde **private mı public mi** seçimi kullanıcıya bırakılabilir; ders politikasına göre private varsayımı güvenli. GitHub Classroom kullanılıyorsa hocanın istediği Classroom repository önceliklidir.

## Son kabul ölçütü

Codex'in "tamamlandı" demesi yeterli değildir. Kabul için gerçek Linux build, tüm temel komutların demosu, no-zombie kontrolü, atomic writer kanıtı, analyst->reporter briefing, düzgün `report.md` ve temiz Git geçmişi gerekir.
