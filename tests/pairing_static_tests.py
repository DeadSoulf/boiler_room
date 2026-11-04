"""Cross-layer guards for browser-initiated, display-only pairing."""
from pathlib import Path
import re,subprocess,tempfile,shutil,os
r=Path(__file__).resolve().parents[1]
read=lambda n:(r/n).read_text(encoding='utf-8')
web,main,page,header=map(read,['src/panel_web.cpp','src/main.cpp','src/panel_web_page.h','src/panel_web.h'])
for route in ('/api/pair/start','/api/pair/cancel','/api/pair'):
 assert 'server.on("'+route+'",HTTP_POST,' in web,route
assert 'panel_pairing::State pairing' in web and 'pairing.verify(ticket.c_str(),entered.c_str(),millis())' in web
assert 'pairingOriginAllowed()' in web and 'server.collectHeaders(headers,4)' in web
start=web[web.index('void pairStartRequest()'):web.index('void pairCancelRequest()')]
assert 'clearSessionLocked' not in start
assert 'jsonQuote(j,candidate)' not in start and 'jsonQuote(j,code)' not in start
status=web[web.index('void statusRequest()'):web.index('void commandRequest()')]
for secret in ('physicalPin','candidate','sessionToken','pairing.display'):
 assert secret not in status,secret
assert 'armPairing' not in main and 'WebAccess' not in main and 'WebView' not in main
assert 'webAccessPage' not in main
assert 'pairBadge=box(header,196,16,132,66' in main
assert 'lv_label_set_text(pairDigits,"")' in main
assert 'shownPairingGeneration=generation' in main
assert 'panel_screen::setTimeout(panel_screen::timeout())' in main
assert 'bool pairingCode(char out[7],uint32_t& remaining,uint32_t* generation=nullptr)' in header
for required in ('async function startPairing()','async function cancelPairing()','/api/pair/start','/api/pair/cancel','request_id:pairTicket','crypto.getRandomValues','<dialog id="pairbox"','id="fuel-canister"'):
 assert required in page,required
assert 'Настройки → Веб-доступ' not in page
home=main[main.index('void home(){'):main.index('void updateEquipmentReadings(){',main.index('void home(){'))]
assert 'menuRow(body,"Уровень топлива"' not in home # Removed from home, retained inside equipment details.
assert 'const char* const PAGES[]={"Главная","Оборудование","Топливо","Настройки"};' in main
compiler=shutil.which(os.environ.get('CXX','g++'))
if compiler:
 with tempfile.TemporaryDirectory() as td:
  exe=Path(td)/'pairing-test'
  subprocess.run([compiler,'-std=c++17','-Wall','-Wextra','-Werror','-pedantic','-I',str(r/'src'),str(r/'tests/pairing_tests.cpp'),'-o',str(exe)],check=True)
  subprocess.run([str(exe)],check=True)
elif os.environ.get('CI'):raise RuntimeError('C++ compiler required in CI')
else:print('SKIP: C++ host compiler absent; state tests run separately under WSL/CI')
print('PASS: exact routes, secret boundary, session preservation, header lifecycle, web form and no physical web-access menu')
