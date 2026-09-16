#pragma once
#include <Arduino.h>   // PROGMEM

// Konfigurationssidan, inbäddad i firmware.
//
// Hållplatssökningen går DIREKT från webbläsaren till SL:s API. Det är möjligt
// eftersom transport.integration.sl.se svarar med
// "access-control-allow-origin: *", och det betyder att ESP32:n aldrig behöver
// röra den 1,35 MB stora hållplatslistan (6 514 poster). Listan hämtas en gång
// och filtreras lokalt i telefonen.
//
// I SoftAP-läge har telefonen ingen internetuppkoppling, så sökningen kan inte
// fungera där. Sidan säger det rakt ut istället för att bara misslyckas.

static const char PORTAL_PAGE[] PROGMEM = R"PAGE(<!doctype html>
<html lang="sv"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Departure Board</title>
<style>
:root{color-scheme:dark;--bg:#111417;--card:#191d21;--line:#2a3036;--fg:#e8eaed;--mut:#9aa3ab;--acc:#ff9c1a}
*{box-sizing:border-box}
body{margin:0;padding:16px;background:var(--bg);color:var(--fg);
 font:15px/1.45 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
.wrap{max-width:520px;margin:0 auto}
.logo{display:block;width:110px;max-width:45%;height:auto;margin:0 auto 10px}
.sub{color:var(--mut);font-size:13px;margin:0 0 18px;text-align:center}
section{background:var(--card);border:1px solid var(--line);border-radius:10px;
 padding:14px;margin-bottom:14px}
h2{font-size:13px;text-transform:uppercase;letter-spacing:.06em;color:var(--mut);
 margin:0 0 12px;font-weight:600}
label{display:block;font-size:13px;color:var(--mut);margin:10px 0 4px}
input,select{width:100%;padding:9px 10px;background:#0d1013;color:var(--fg);
 border:1px solid var(--line);border-radius:7px;font-size:15px}
input:focus,select:focus{outline:2px solid var(--acc);outline-offset:-1px}
button{width:100%;padding:12px;background:var(--acc);color:#18120a;border:0;
 border-radius:8px;font-size:15px;font-weight:650;cursor:pointer;margin-top:14px}
button.sec{background:#252b31;color:var(--fg);font-weight:500}
button:disabled{opacity:.5;cursor:default}
.row{display:flex;gap:10px}.row>*{flex:1}
.chips{display:flex;flex-wrap:wrap;gap:8px;margin-top:6px}
.chip{padding:7px 12px;border:1px solid var(--line);border-radius:999px;
 font-size:13px;cursor:pointer;user-select:none;background:#0d1013}
.chip.on{background:var(--acc);color:#18120a;border-color:var(--acc)}
.hits{margin-top:8px;max-height:210px;overflow:auto;border:1px solid var(--line);
 border-radius:7px}
.hit{padding:9px 11px;border-bottom:1px solid var(--line);cursor:pointer;font-size:14px}
.hit:last-child{border-bottom:0}
.hit small{color:var(--mut)}
.note{font-size:13px;color:var(--mut);margin-top:8px}
.warn{font-size:13px;color:#ffcf5c;background:#2a2314;border:1px solid #4a3d1c;
 border-radius:7px;padding:10px;margin-top:10px}
.ok{color:#7ee08a}.err{color:#ff7b72}
.cur{font-size:14px;margin-top:2px}
</style></head><body><div class="wrap">

<img class="logo" src="/logo.svg" alt="MB Labs">
<p class="sub" id="status">Laddar...</p>

<section id="wifiCard">
<h2>WiFi</h2>
<label for="ssid">Nätverk</label>
<div class="row">
  <select id="ssid"><option value="">-- välj --</option></select>
  <button class="sec" style="flex:0 0 96px;margin:0" id="scan">Sök</button>
</div>
<label for="pass">Lösenord</label>
<input id="pass" type="password" autocomplete="off" placeholder="lämna tomt för öppet nät">
<button id="saveWifi">Anslut</button>
<p class="note" id="wifiNote"></p>
</section>

<section>
<h2>Hållplats</h2>
<p class="cur">Vald: <strong id="curSite">-</strong></p>
<label for="q">Sök hållplats</label>
<input id="q" placeholder="t.ex. Gullmarsplan" autocomplete="off">
<div class="hits" id="hits" hidden></div>
<p class="note" id="siteNote"></p>
</section>

<section>
<h2>Avgångar</h2>
<label>Trafikslag <span class="note">(inget valt = alla)</span></label>
<div class="chips" id="modes"></div>
<label for="dir">Riktning</label>
<select id="dir">
  <option value="0">Båda</option><option value="1">1</option><option value="2">2</option>
</select>
<label for="walk">Gångtid till hållplatsen (min)</label>
<input id="walk" type="number" min="0" max="60" step="1">
<p class="note">Avgångar som går tidigare än så döljs.</p>
</section>

<section>
<h2>Utseende</h2>
<label for="bri">Ljusstyrka <span id="briVal"></span></label>
<input id="bri" type="range" min="0" max="255">
<label for="col">Färgtema</label>
<!-- Fylls från /api/settings: enheten äger listan, sidan har ingen egen kopia -->
<select id="col"></select>
</section>

<button id="save">Spara</button>
<p class="note" id="saveNote"></p>
</div>

<script>
var S = {}, sites = null, loading = false;
var MODES = [["BUS","Buss",1],["METRO","Tunnelbana",2],["TRAIN","Pendeltåg",4],
             ["TRAM","Spårvagn",8],["SHIP","Båt",16]];

function $(id){ return document.getElementById(id); }
function j(u,o){ return fetch(u,o).then(function(r){
  if(!r.ok) throw new Error(r.status); return r.json(); }); }

function renderModes(){
  var c = $("modes"); c.innerHTML = "";
  MODES.forEach(function(m){
    var d = document.createElement("div");
    d.className = "chip" + ((S.transportMask & m[2]) ? " on" : "");
    d.textContent = m[1];
    d.onclick = function(){ S.transportMask ^= m[2]; renderModes(); };
    c.appendChild(d);
  });
}

function load(){
  j("/api/settings").then(function(s){
    S = s;
    // Namnet fylls i av enheten ur avgangssvaret; direkt efter en
    // hallplatsandring kan det dröja till nasta hamtning.
    $("curSite").textContent = s.siteName
      ? s.siteName + " (" + s.siteId + ")"
      : "hallplats " + s.siteId;
    $("dir").value = s.directionCode;
    $("walk").value = s.walkMinutes;
    $("bri").value = s.brightness;
    $("briVal").textContent = Math.round(s.brightness*100/255) + "%";
    var sel = $("col");
    sel.innerHTML = "";
    (s.themes || []).forEach(function(th){
      var o = document.createElement("option");
      o.value = th.v; o.textContent = th.n;
      sel.appendChild(o);
    });
    sel.value = s.colourway;
    renderModes();
    return j("/api/status");
  }).then(function(st){
    if(st.ap){
      $("status").innerHTML = "Enheten kör sitt eget nätverk. <b>Välj hemnätverk nedan först</b> — hållplatssökning kräver internet.";
      $("wifiNote").innerHTML = '<span class="note">Efter anslutning: återgå till ditt hemnätverk och skanna QR-koden på panelen igen.</span>';
    } else {
      $("status").innerHTML = 'Ansluten till <b>' + st.ssid + '</b><br>' + st.ip;
      // Nätverket är redan valt. Vill man byta: håll inne encodern under
      // uppstart för att tvinga fram enhetens eget nät igen.
      $("wifiCard").hidden = true;
    }
  }).catch(function(e){ $("status").innerHTML = '<span class="err">Kunde inte läsa inställningar</span>'; });
}

$("bri").oninput = function(){ $("briVal").textContent = Math.round(this.value*100/255) + "%"; };

$("scan").onclick = function(){
  var b = this; b.disabled = true; b.textContent = "...";
  j("/api/wifi/scan").then(function(list){
    var s = $("ssid"); s.innerHTML = '<option value="">-- välj --</option>';
    list.forEach(function(n){
      var o = document.createElement("option");
      o.value = n.ssid; o.textContent = n.ssid + "  (" + n.rssi + " dBm)";
      s.appendChild(o);
    });
  }).catch(function(){ $("wifiNote").innerHTML = '<span class="err">Sökning misslyckades</span>'; })
   .then(function(){ b.disabled = false; b.textContent = "Sök"; });
};

$("saveWifi").onclick = function(){
  var ssid = $("ssid").value;
  if(!ssid){ $("wifiNote").innerHTML = '<span class="err">Välj ett nätverk</span>'; return; }
  this.disabled = true;
  j("/api/wifi", {method:"POST", headers:{"Content-Type":"application/json"},
      body: JSON.stringify({ssid: ssid, pass: $("pass").value})})
   .then(function(){ $("wifiNote").innerHTML = '<span class="ok">Sparat. Enheten ansluter — se panelen för IP-adress.</span>'; })
   .catch(function(){ $("wifiNote").innerHTML = '<span class="err">Kunde inte spara</span>'; })
   .then(function(){ $("saveWifi").disabled = false; });
};

function ensureSites(){
  if(sites) return Promise.resolve(sites);
  if(loading) return Promise.reject();
  loading = true;
  $("siteNote").textContent = "Hämtar hållplatslistan (1,3 MB, en gång)…";
  return fetch("https://transport.integration.sl.se/v1/sites?expand=false")
    .then(function(r){ return r.json(); })
    .then(function(list){
      sites = list; loading = false;
      $("siteNote").textContent = list.length + " hållplatser laddade.";
      return list;
    })
    .catch(function(e){
      loading = false;
      $("siteNote").innerHTML = '<div class="warn">Kunde inte hämta hållplatslistan. Telefonen behöver internet — anslut enheten till ditt WiFi först, återgå sedan till ditt hemnätverk och öppna den här sidan via enhetens IP-adress.</div>';
      throw e;
    });
}

var t = null;
$("q").oninput = function(){
  clearTimeout(t);
  var v = this.value.trim().toLowerCase();
  if(v.length < 2){ $("hits").hidden = true; return; }
  t = setTimeout(function(){
    ensureSites().then(function(list){
      var out = [], i;
      for(i = 0; i < list.length && out.length < 40; i++){
        if((list[i].name || "").toLowerCase().indexOf(v) >= 0) out.push(list[i]);
      }
      var h = $("hits"); h.innerHTML = ""; h.hidden = false;
      if(!out.length){ h.innerHTML = '<div class="hit">Inga träffar</div>'; return; }
      out.forEach(function(s){
        var d = document.createElement("div");
        d.className = "hit";
        d.innerHTML = s.name + " <small>" + (s.note || "") + " - " + s.id + "</small>";
        d.onclick = function(){
          S.siteId = s.id; S.siteName = s.name;
          $("curSite").textContent = s.name + " (" + s.id + ")";
          h.hidden = true; $("q").value = "";
        };
        h.appendChild(d);
      });
    }).catch(function(){});
  }, 250);
};

$("save").onclick = function(){
  S.directionCode = +$("dir").value;
  S.walkMinutes   = +$("walk").value;
  S.brightness    = +$("bri").value;
  S.colourway     = +$("col").value;
  this.disabled = true;
  j("/api/settings", {method:"POST", headers:{"Content-Type":"application/json"},
      body: JSON.stringify(S)})
   .then(function(){ $("saveNote").innerHTML = '<span class="ok">Sparat — panelen uppdateras direkt.</span>'; })
   .catch(function(){ $("saveNote").innerHTML = '<span class="err">Kunde inte spara</span>'; })
   .then(function(){ $("save").disabled = false; });
};

load();
</script></body></html>
)PAGE";
