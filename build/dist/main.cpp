#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <cstdlib>
#include <cctype>
#include <functional>
#include <tgbot/tgbot.h>
#include <swephexp.h>
#include "plugin_api.hpp"
#include <map>
#include <thread>
#include <mutex>
#include <fstream>
#include <filesystem>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#ifdef ASTRONOMIAL_GUI
#include <windows.h>
#endif
using socket_t = SOCKET;
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
using socket_t = int;
#endif
#define SE_KETU 100
using namespace std;
struct KPLords { int signLord; int starLord; int subLord; };
struct KPPosition { int id; string name; double longitude; int house; KPLords lords; };
struct KPHouse { int houseNumber; double cuspDegrees; KPLords lords; vector<KPPosition> occupants; };
int getSignLord(int signIndex) { const int signRulers[] = { SE_MARS, SE_VENUS, SE_MERCURY, SE_MOON, SE_SUN, SE_MERCURY, SE_VENUS, SE_MARS, SE_JUPITER, SE_SATURN, SE_SATURN, SE_JUPITER }; return signRulers[signIndex]; }
KPLords computeKPLords(double longitude) { KPLords lords; lords.subLord = -1; const double NAKSHATRA_SPAN = 13.333333333333334; const double DASA_PERIODS[] = { 7.0, 20.0, 6.0, 10.0, 7.0, 18.0, 16.0, 19.0, 17.0 }; const int PLANET_ORDER[] = { SE_KETU, SE_VENUS, SE_SUN, SE_MOON, SE_MARS, SE_MEAN_NODE, SE_JUPITER, SE_SATURN, SE_MERCURY }; int signIndex = static_cast<int>(longitude / 30.0); lords.signLord = getSignLord(signIndex); int totalStarIndex = static_cast<int>(longitude / NAKSHATRA_SPAN); lords.starLord = PLANET_ORDER[totalStarIndex % 9]; double positionInStar = longitude - (totalStarIndex * NAKSHATRA_SPAN); double accumulatedDegrees = 0.0; int currentLordIndex = totalStarIndex % 9; for (int i = 0; i < 9; ++i) { int targetIndex = (currentLordIndex + i) % 9; double subArc = (DASA_PERIODS[targetIndex] / 120.0) * NAKSHATRA_SPAN; accumulatedDegrees += subArc; if (positionInStar <= accumulatedDegrees) { lords.subLord = PLANET_ORDER[targetIndex]; break; } } return lords; }
bool isPlanetInHouse(double pLong, double startCusp, double endCusp) { if (startCusp < endCusp) return (pLong >= startCusp && pLong < endCusp); else return (pLong >= startCusp || pLong < endCusp); }
string pn(int id, bool full = false) { switch (id) { case SE_SUN: return full ? "Sun" : "Su"; case SE_MOON: return full ? "Moon" : "Mo"; case SE_MARS: return full ? "Mars" : "Ma"; case SE_MERCURY: return full ? "Mercury" : "Me"; case SE_JUPITER: return full ? "Jupiter" : "Ju"; case SE_VENUS: return full ? "Venus" : "Ve"; case SE_SATURN: return full ? "Saturn" : "Sa"; case SE_MEAN_NODE: return full ? "Rahu" : "Ra"; case SE_KETU: return full ? "Ketu" : "Ke"; } return "??"; }
string fmtPos(double lon) { const char* SIGNS[] = { "Ari", "Tau", "Gem", "Can", "Leo", "Vir", "Lib", "Sco", "Sag", "Cap", "Aqu", "Pis" }; lon = fmod(lon, 360.0); if (lon < 0) lon += 360.0; int s = static_cast<int>(lon / 30.0); double d = lon - s * 30.0; int dd = static_cast<int>(d); int mm = static_cast<int>((d - dd) * 60.0); stringstream o; o << SIGNS[s] << " " << setw(2) << dd << ":" << setfill('0') << setw(2) << mm; return o.str(); }
string lordsStr(KPLords l) { return pn(l.signLord) + "-" + pn(l.starLord) + "-" + pn(l.subLord); }
string usageMsg() { return "Format:\n<code>/birth_date DD-MM-YYYY HH:MM:SS LatDeg LatMin LatSec N/S LonDeg LonMin LonSec E/W</code>\nTime must be in UT (GMT), not local time.\nExample:\n<code>/birth_date 15-08-1995 04:30:00 51 30 26 N 0 7 39 W</code>"; }
string helpMsg() { return "<b>Astronomial</b> - a KP astrology bot\n\n/birth_date - full chart (houses and planets)\n/houses - only the houses\n/planets - only the planets\n/lords DEGREES - KP lords of one degree\n/example - an example you can copy\n/about - what is KP?\n/help - this message\n\nChart commands use this format:\n<code>/birth_date DD-MM-YYYY HH:MM:SS LatDeg LatMin LatSec N/S LonDeg LonMin LonSec E/W</code>\nTime must be in UT (GMT), not local time."; }
string exampleMsg() { return "Copy and send this:\n<code>/birth_date 15-08-1995 04:30:00 51 30 26 N 0 7 39 W</code>\n\nTry /houses or /planets with the same numbers, or <code>/lords 123.45</code>"; }
string aboutMsg() { return "<b>About KP</b>\nKP (Krishnamurti Paddhati) splits every star (nakshatra) into 9 sub-parts. Every point gets 3 lords: sign, star and sub. In the chart they are shown as Sg-St-Sb, for example Ve-Su-Mo.\nThis bot uses the Placidus houses and a KP-style ayanamsa."; }
void sendHtml(TgBot::Bot& bot, int64_t chatId, const string& text) { try { bot.getApi().sendMessage(chatId, text, nullptr, nullptr, {}, "HTML"); } catch (exception& e) { cout << e.what() << endl; } }
struct AyanamsaOption {
    const char* label;
    int siderealMode;
    bool tropical;
    bool larryEly;
};

static const AyanamsaOption ayanamsaOptions[] = {
    {"Astronomial KP (custom)", SE_SIDM_USER, false, false},
    {"Lahiri (Chitra Paksha)", SE_SIDM_LAHIRI, false, false},
    {"Raman", SE_SIDM_RAMAN, false, false},
    {"Krishnamurti (KP)", SE_SIDM_KRISHNAMURTI, false, false},
    {"Fagan-Bradley", SE_SIDM_FAGAN_BRADLEY, false, false},
    {"Pushya Paksha", SE_SIDM_TRUE_PUSHYA, false, false},
    {"Yukteshwar", SE_SIDM_YUKTESHWAR, false, false},
    {"Suryasiddhanta", SE_SIDM_SURYASIDDHANTA, false, false},
    {"True Chitra", SE_SIDM_TRUE_CITRA, false, false},
    {"Bhasin", SE_SIDM_JN_BHASIN, false, false},
    {"Larry Ely", SE_SIDM_USER, false, true},
    {"Galactic Center (0 Sagittarius)", SE_SIDM_GALCENT_0SAG, false, false},
    {"Tropical (0 Aries)", 0, true, false}
};

static int selectedAyanamsa = 0;

static bool selectAyanamsa(const string& requested) {
    string normalized = requested;
    transform(normalized.begin(), normalized.end(), normalized.begin(),
        [](unsigned char c) { return static_cast<char>(tolower(c)); });
    for (size_t index = 0; index < size(ayanamsaOptions); ++index) {
        string label = ayanamsaOptions[index].label;
        transform(label.begin(), label.end(), label.begin(),
            [](unsigned char c) { return static_cast<char>(tolower(c)); });
        if (normalized == label || (index == 0 && normalized == "kp") ||
            (index == 3 && normalized == "krishnamurti") ||
            (index == 11 && normalized == "galactic-center")) {
            selectedAyanamsa = static_cast<int>(index);
            return true;
        }
    }
    return false;
}

static string buildChart(const string& text, int mode) {
    stringstream ss(text);
    string command;
    int day, month, year, hour, minute, second;
    int latDeg, latMin, lonDeg, lonMin;
    double latSec, lonSec;
    char dash1, dash2, colon1, colon2, latDir, lonDir;
    ss >> command >> day >> dash1 >> month >> dash2 >> year >> hour >> colon1 >> minute >> colon2 >> second >> latDeg >> latMin >> latSec >> latDir >> lonDeg >> lonMin >> lonSec >> lonDir;
    if (ss.fail()) return usageMsg();
    if (month < 1 || month > 12 || day < 1 || day > 31 || hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) return usageMsg();
    if (string("NSns").find(latDir) == string::npos || string("EWew").find(lonDir) == string::npos) return usageMsg();

    double latitude = latDeg + (latMin / 60.0) + (latSec / 3600.0);
    if (latDir == 'S' || latDir == 's') latitude = -latitude;
    double longitude = lonDeg + (lonMin / 60.0) + (lonSec / 3600.0);
    if (lonDir == 'W' || lonDir == 'w') longitude = -longitude;
    double decimalHour = hour + (minute / 60.0) + (second / 3600.0);
    double julianDay = swe_julday(year, month, day, decimalHour, SE_GREG_CAL);
    int birthYear, dummyMonth, dummyDay;
    double dummyHour;
    swe_revjul(julianDay, SE_GREG_CAL, &birthYear, &dummyMonth, &dummyDay, &dummyHour);

    double ayanamsaValue = 0.0;
    const AyanamsaOption& option = ayanamsaOptions[selectedAyanamsa];
    int32 flags = SEFLG_SPEED;
    if (!option.tropical) {
        flags |= SEFLG_SIDEREAL;
        if (option.larryEly) {
            double referenceDate = swe_julday(-12, 7, 1, 0, SE_GREG_CAL);
            swe_set_sid_mode(SE_SIDM_USER, referenceDate, 26.72);
            ayanamsaValue = swe_get_ayanamsa_ut(julianDay);
        } else if (option.siderealMode == SE_SIDM_USER) {
            double years = birthYear - 1900.0;
            double baseAyanamsa = 22.0 + (1350.0 / 3600.0);
            double precessionSeconds = (years * 50.2388475) + (years * years * 0.000111);
            ayanamsaValue = baseAyanamsa + (precessionSeconds / 3600.0);
            swe_set_sid_mode(SE_SIDM_USER, julianDay, ayanamsaValue);
        } else {
            swe_set_sid_mode(option.siderealMode, 0, 0);
            ayanamsaValue = swe_get_ayanamsa_ut(julianDay);
        }
    }

    double cusps[13];
    double ascmc[10];
    if (swe_houses_ex(julianDay, flags, latitude, longitude, 'P', cusps, ascmc) < 0)
        return "Could not calculate the houses.";
    vector<KPHouse> chart(12);
    for (int house = 1; house <= 12; ++house) {
        chart[house - 1].houseNumber = house;
        chart[house - 1].cuspDegrees = cusps[house];
        chart[house - 1].lords = computeKPLords(cusps[house]);
    }

    vector<pair<int, string>> targets = {
        {SE_SUN, "Sun"}, {SE_MOON, "Moon"}, {SE_MERCURY, "Mercury"},
        {SE_VENUS, "Venus"}, {SE_MARS, "Mars"}, {SE_JUPITER, "Jupiter"},
        {SE_SATURN, "Saturn"}, {SE_MEAN_NODE, "Rahu"}, {SE_KETU, "Ketu"}
    };
    vector<KPPosition> calculatedPlanets;
    for (const auto& target : targets) {
        double position[6];
        char error[256];
        int calculationId = target.first == SE_KETU ? SE_MEAN_NODE : target.first;
        if (swe_calc_ut(julianDay, calculationId, flags, position, error) >= 0) {
            KPPosition planet;
            planet.id = target.first;
            planet.name = target.second;
            planet.house = 0;
            planet.longitude = position[0];
            if (target.first == SE_KETU) planet.longitude = fmod(position[0] + 180.0, 360.0);
            planet.lords = computeKPLords(planet.longitude);
            calculatedPlanets.push_back(planet);
        }
    }
    for (auto& planet : calculatedPlanets) {
        for (int house = 0; house < 12; ++house) {
            double startCusp = chart[house].cuspDegrees;
            double endCusp = chart[(house + 1) % 12].cuspDegrees;
            if (isPlanetInHouse(planet.longitude, startCusp, endCusp)) {
                planet.house = house + 1;
                chart[house].occupants.push_back(planet);
                break;
            }
        }
    }

    stringstream output;
    output << fixed << setprecision(4);
    output << "<b>KP Chart</b>\n<pre>";
    output << "Julian Day: " << julianDay << "\n";
    output << "Lat / Lon : " << latitude << " / " << longitude << "\n";
    output << "Ayanamsa  : " << option.label << "\n";
    output << "Ayan value: " << ayanamsaValue << "\n";
    output << "Lords = sign-star-sub\n";
    if (mode != 1) {
        output << "\nHOUSES\n H  Cusp        Lords     Planets\n";
        for (const auto& house : chart) {
            output << setw(2) << house.houseNumber << "  " << fmtPos(house.cuspDegrees) << "  " << lordsStr(house.lords) << "  ";
            if (house.occupants.empty()) output << "-";
            else for (const auto& planet : house.occupants) output << pn(planet.id) << " ";
            output << "\n";
        }
    }
    if (mode != 2) {
        output << "\nPLANETS\nPl  Position   Lords     H\n";
        for (const auto& planet : calculatedPlanets)
            output << pn(planet.id) << "  " << fmtPos(planet.longitude) << "  " << lordsStr(planet.lords) << "  " << planet.house << "\n";
    }
    output << "</pre>";
    return output.str();
}

namespace fs = std::filesystem;
using astronomial::CommandRequest;
using astronomial::CommandResponse;
using astronomial::PluginCommand;
using astronomial::PluginCommands;

static void closeSocket(socket_t s) {
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}

class CommandRouter {
    std::map<std::string, PluginCommands*> plugins;
    std::map<std::string, PluginCommand> descriptions;
public:
    void registerPlugin(PluginCommands* plugin) {
        if (!plugin) return;
        for (const auto& command : plugin->commands()) {
            if (descriptions.count(command.name)) throw runtime_error("duplicate command: " + command.name);
            descriptions[command.name] = command;
        }
        plugins[plugin->commands().empty() ? "" : plugin->commands().front().plugin_id] = plugin;
    }
    vector<PluginCommand> commands() const {
        vector<PluginCommand> out;
        for (const auto& [name, command] : descriptions) out.push_back(command);
        return out;
    }
    CommandResponse dispatch(const CommandRequest& request) const {
        for (const auto& [id, plugin] : plugins) {
            for (const auto& command : plugin->commands()) {
                if (command.name == request.name) return plugin->run(request);
            }
        }
        return {127, "", "unknown command: " + request.name};
    }
};

class CoreCommands final : public PluginCommands {
public:
    using Handler = function<CommandResponse(const CommandRequest&)>;
private:
    vector<PluginCommand> specs;
    map<string, Handler> handlers;
public:
    void add(string name, string description, Handler handler) {
        specs.push_back({"astronomial.core", move(name), move(description), {}});
        handlers[specs.back().name] = move(handler);
    }
    vector<PluginCommand> commands() const override { return specs; }
    CommandResponse run(const CommandRequest& request) override {
        auto it = handlers.find(request.name);
        if (it == handlers.end()) return {127, "", "unknown command: " + request.name};
        return it->second(request);
    }
};

static CommandResponse coreCommand(const string& name, const string& args) {
    if (name == "help") return {0, helpMsg(), ""};
    if (name == "example") return {0, exampleMsg(), ""};
    if (name == "about") return {0, aboutMsg(), ""};
    if (name == "birth_date" || name == "planets" || name == "houses") {
        int mode = name == "planets" ? 1 : name == "houses" ? 2 : 0;
        return {0, buildChart(name + " " + args, mode), ""};
    }
    if (name == "lords") {
        stringstream ss(args); double deg;
        ss >> deg;
        if (ss.fail() || deg < 0 || deg >= 360) return {2, "", "Use: lords DEGREES (0 to 359.99)"};
        KPLords l = computeKPLords(deg); stringstream o;
        o << "KP lords\n" << fmtPos(deg) << "\nSign lord: " << pn(l.signLord, true)
          << "\nStar lord: " << pn(l.starLord, true) << "\nSub lord: " << pn(l.subLord, true);
        return {0, o.str(), ""};
    }
    return {127, "", "unknown command: " + name};
}

static vector<string> splitArgs(const string& text) {
    vector<string> out; string word; bool quoted = false;
    for (char c : text) {
        if (c == '"') quoted = !quoted;
        else if (isspace(static_cast<unsigned char>(c)) && !quoted) { if (!word.empty()) { out.push_back(word); word.clear(); } }
        else word += c;
    }
    if (!word.empty()) out.push_back(word);
    return out;
}

static string htmlPage() {
    return R"HTML(<!doctype html><html><head><meta charset="utf-8"><title>Astronomial</title>
<style>body{font:16px system-ui;max-width:900px;margin:40px auto;padding:0 20px}textarea{width:100%;height:100px}button{padding:8px 14px}pre{white-space:pre-wrap;background:#f3f3f3;padding:14px;border-radius:8px}</style></head>
<body><h1>Astronomial</h1><p>Shared command interface</p><select id="cmd"></select><textarea id="args" placeholder="Command arguments"></textarea><button onclick="run()">Run</button><pre id="out"></pre>
<script>fetch('/api/commands').then(r=>r.json()).then(a=>{cmd.innerHTML=a.map(x=>`<option value="${x.name}">${x.name} — ${x.description}</option>`).join('')});async function run(){let r=await fetch('/api/run?command='+encodeURIComponent(cmd.value),{method:'POST',body:args.value});out.textContent=await r.text()}</script></body></html>)HTML";
}

static string urlDecode(string s) {
    string out; char hex[3]{};
    for (size_t i=0;i<s.size();++i) { if (s[i]=='%' && i+2<s.size()) { hex[0]=s[i+1];hex[1]=s[i+2];out += static_cast<char>(strtol(hex,nullptr,16));i+=2; } else if(s[i]=='+') out+=' '; else out+=s[i]; }
    return out;
}

static void serveWeb(CommandRouter& router, const string& host, int port) {
#ifdef _WIN32
    WSADATA wsa{}; WSAStartup(MAKEWORD(2,2), &wsa);
#endif
    socket_t server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) throw runtime_error("could not create web socket");
    int yes=1; setsockopt(server, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));
    sockaddr_in addr{}; addr.sin_family=AF_INET; addr.sin_port=htons(static_cast<uint16_t>(port));
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
    if (bind(server, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0 || listen(server, 16) < 0) { closeSocket(server); throw runtime_error("could not bind web server"); }
    cout << "Web UI: http://" << host << ":" << port << endl;
    while (true) {
        socket_t client = accept(server, nullptr, nullptr); if (client < 0) continue;
        char buffer[16384]{}; int n = recv(client, buffer, sizeof(buffer)-1, 0); if (n <= 0) { closeSocket(client); continue; }
        string req(buffer, n), first = req.substr(0, req.find("\r\n"));
        string body; auto bodyPos=req.find("\r\n\r\n"); if(bodyPos!=string::npos) body=req.substr(bodyPos+4);
        string response, type="text/plain; charset=utf-8";
        if (first.rfind("GET / ",0)==0) { response=htmlPage(); type="text/html; charset=utf-8"; }
        else if (first.find("GET /api/commands") == 0) {
            response="["; bool firstItem=true; for(auto& c:router.commands()){if(!firstItem)response+=",";firstItem=false;response+="{\"name\":\""+c.name+"\",\"description\":\""+c.description+"\"}";} response+="]"; type="application/json";
        } else if (first.find("POST /api/run") == 0) {
            auto q=first.find("command="); string name=q==string::npos?"":urlDecode(first.substr(q+8).substr(0, first.substr(q+8).find(' ')));
            auto r=router.dispatch({name, splitArgs(body), body, "web"}); response=r.exit_code? r.error:r.output;
        } else response="Not found";
        string http="HTTP/1.1 "+string(response=="Not found"?"404 Not Found":"200 OK")+"\r\nContent-Type: "+type+"\r\nContent-Length: "+to_string(response.size())+"\r\nConnection: close\r\n\r\n"+response;
        send(client,http.c_str(),static_cast<int>(http.size()),0); closeSocket(client);
    }
}

static int runCli(CommandRouter& router, int argc, char** argv) {
    if (argc < 3) { cout << "Usage: astro.exe cli <command> [arguments]\n"; return 2; }
    string name=argv[2], args;
    bool addSeparator = false;
    for (int i = 3; i < argc; ++i) {
        if (string(argv[i]) == "--ayanamsa") {
            string requestedAyanamsa;
            while (i + 1 < argc && string(argv[i + 1]).rfind("--", 0) != 0) {
                if (!requestedAyanamsa.empty()) requestedAyanamsa += ' ';
                requestedAyanamsa += argv[++i];
            }
            if (requestedAyanamsa.empty() || !selectAyanamsa(requestedAyanamsa)) {
                cerr << "Unknown ayanamsa. Use --ayanamsa followed by a listed ayanamsa name.\n";
                return 2;
            }
            continue;
        }
        if (addSeparator) args += ' ';
        args += argv[i];
        addSeparator = true;
    }
    auto r=router.dispatch({name,splitArgs(args),args,"cli"}); if(r.exit_code) cerr<<r.error<<"\n"; else cout<<r.output<<"\n"; return r.exit_code;
}

static string telegramArguments(const string& text) {
    auto firstSpace = text.find_first_of(" \t\n\r");
    return firstSpace == string::npos ? string() : text.substr(firstSpace + 1);
}

static string shellQuote(const string& value) {
    string quoted = "\"";
    for (char c : value) {
        if (c == '"') quoted += '\\';
        quoted += c;
    }
    return quoted + "\"";
}

static int runRegistration(int argc, char** argv) {
    if (argc < 4) {
        cerr << "Usage: astro.exe register new plugin --author <Creator Name> --name <Plugin Name> [--nonce <Optional>]\n"
             << "       astro.exe register plugindist --hash <UID hash>\n";
        return 2;
    }
    string helper = (string("python tools/register_plugin.py ") +
                     (string(argv[2]) == "new" && string(argv[3]) == "plugin" ? "new-plugin" :
                      string(argv[2]) == "plugindist" ? "validate-uid" : ""));
    if (helper.back() == ' ') {
        cerr << "Unknown registration action.\n";
        return 2;
    }
    int firstArgument = string(argv[2]) == "plugindist" ? 3 : 4;
    for (int i = firstArgument; i < argc; ++i) helper += " " + shellQuote(argv[i]);
    return system(helper.c_str());
}

#define plugin_key astronomial::ASTRONOMIAL_PLUGIN_KEY_ID
static void initializeEphemeris() {
    string ephePath = "./ephe";
    const char* localAppData = getenv("LOCALAPPDATA");
    if (localAppData && *localAppData) {
        ephePath = string(localAppData) + "/.astronomial/ephe";
    }
    swe_set_ephe_path(ephePath.c_str());
}

#ifdef ASTRONOMIAL_GUI
enum GuiControlId {
    GuiMode = 1001,
    GuiDate,
    GuiTime,
    GuiLatitude,
    GuiLongitude,
    GuiDegrees,
    GuiCalculate,
    GuiExample,
    GuiCopy,
    GuiOutput,
    GuiAyanamsa
};

static HWND guiMode;
static HWND guiAyanamsa;
static HWND guiDate;
static HWND guiTime;
static HWND guiLatitude;
static HWND guiLongitude;
static HWND guiDegrees;
static HWND guiOutput;
static HFONT guiFont;
static HFONT guiOutputFont;

static HWND createGuiControl(HWND parent, const char* type, const char* text,
    DWORD style, int x, int y, int width, int height, int id) {
    HWND control = CreateWindowExA(0, type, text, WS_CHILD | WS_VISIBLE | style,
        x, y, width, height, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleA(nullptr), nullptr);
    if (guiFont && control) SendMessageA(control, WM_SETFONT, reinterpret_cast<WPARAM>(guiFont), TRUE);
    return control;
}

static string readGuiText(HWND control) {
    int length = GetWindowTextLengthA(control);
    vector<char> text(static_cast<size_t>(length) + 1);
    GetWindowTextA(control, text.data(), length + 1);
    return string(text.data());
}

static void writeGuiText(HWND control, const string& text) {
    SetWindowTextA(control, text.c_str());
}

static bool parseGuiNumber(const string& text, double& value) {
    stringstream input(text);
    input >> value;
    input >> ws;
    return !input.fail() && input.eof() && isfinite(value);
}

static string formatCoordinate(double coordinate, char positiveDirection, char negativeDirection) {
    long long scaledSeconds = llround(abs(coordinate) * 360000.0);
    long long degrees = scaledSeconds / 360000;
    long long remainder = scaledSeconds % 360000;
    long long minutes = remainder / 6000;
    double seconds = (remainder % 6000) / 100.0;
    char direction = coordinate < 0 ? negativeDirection : positiveDirection;
    stringstream output;
    output << degrees << ' ' << minutes << ' ' << fixed << setprecision(2) << seconds << ' ' << direction;
    return output.str();
}

static void copyGuiOutput(HWND window) {
    string text = readGuiText(guiOutput);
    if (!OpenClipboard(window)) return;
    EmptyClipboard();
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
    if (memory) {
        void* target = GlobalLock(memory);
        if (target) {
            memcpy(target, text.c_str(), text.size() + 1);
            GlobalUnlock(memory);
            if (!SetClipboardData(CF_TEXT, memory)) GlobalFree(memory);
        } else {
            GlobalFree(memory);
        }
    }
    CloseClipboard();
}

static void calculateGuiResult(HWND window) {
    int selection = static_cast<int>(SendMessageA(guiMode, CB_GETCURSEL, 0, 0));
    const char* commands[] = {"birth_date", "planets", "houses", "lords"};
    if (selection < 0 || selection >= 4) selection = 0;
    string arguments;
    if (selection == 3) {
        double degrees;
        if (!parseGuiNumber(readGuiText(guiDegrees), degrees) || degrees < 0 || degrees >= 360) {
            writeGuiText(guiOutput, "Enter a KP longitude from 0 up to (but not including) 360 degrees.");
            return;
        }
        stringstream formatted;
        formatted << setprecision(12) << degrees;
        arguments = formatted.str();
    } else {
        double latitude;
        double longitude;
        if (!parseGuiNumber(readGuiText(guiLatitude), latitude) || abs(latitude) > 90) {
            writeGuiText(guiOutput, "Latitude must be a decimal degree value between -90 and 90.");
            return;
        }
        if (!parseGuiNumber(readGuiText(guiLongitude), longitude) || abs(longitude) > 180) {
            writeGuiText(guiOutput, "Longitude must be a decimal degree value between -180 and 180.");
            return;
        }
        arguments = readGuiText(guiDate) + " " + readGuiText(guiTime) + " " +
            formatCoordinate(latitude, 'N', 'S') + " " +
            formatCoordinate(longitude, 'E', 'W');
    }
    CommandResponse response = coreCommand(commands[selection], arguments);
    writeGuiText(guiOutput, response.exit_code ? response.error : response.output);
    SetFocus(guiOutput);
    (void)window;
}

static void layoutGui(HWND window) {
    RECT client{};
    GetClientRect(window, &client);
    int width = max(720L, client.right - client.left);
    int height = max(560L, client.bottom - client.top);
    MoveWindow(guiOutput, 24, 322, width - 48, height - 346, TRUE);
}

static LRESULT CALLBACK guiWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        guiFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        guiOutputFont = CreateFontA(-17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            FIXED_PITCH | FF_MODERN, "Consolas");
        createGuiControl(window, "STATIC", "Calculation", 0, 24, 22, 110, 22, 0);
        guiMode = createGuiControl(window, "COMBOBOX", "", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,
            140, 18, 260, 180, GuiMode);
        SendMessageA(guiMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Full chart"));
        SendMessageA(guiMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Planets"));
        SendMessageA(guiMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Houses"));
        SendMessageA(guiMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("KP lords"));
        SendMessageA(guiMode, CB_SETCURSEL, 0, 0);
        createGuiControl(window, "STATIC", "Ayanamsa", 0, 426, 22, 74, 20, 0);
        guiAyanamsa = createGuiControl(window, "COMBOBOX", "",
            CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 500, 18, 300, 220, GuiAyanamsa);
        for (const auto& option : ayanamsaOptions)
            SendMessageA(guiAyanamsa, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(option.label));
        SendMessageA(guiAyanamsa, CB_SETCURSEL, selectedAyanamsa, 0);

        createGuiControl(window, "STATIC", "Birth date (DD-MM-YYYY)", 0, 24, 66, 190, 20, 0);
        guiDate = createGuiControl(window, "EDIT", "", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,
            220, 62, 160, 26, GuiDate);
        createGuiControl(window, "STATIC", "UT time (HH:MM:SS)", 0, 410, 66, 160, 20, 0);
        guiTime = createGuiControl(window, "EDIT", "", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,
            570, 62, 140, 26, GuiTime);
        createGuiControl(window, "STATIC", "Latitude (decimal degrees)", 0, 24, 112, 200, 20, 0);
        guiLatitude = createGuiControl(window, "EDIT", "", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,
            220, 108, 160, 26, GuiLatitude);
        createGuiControl(window, "STATIC", "Longitude (decimal degrees)", 0, 410, 112, 200, 20, 0);
        guiLongitude = createGuiControl(window, "EDIT", "", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,
            610, 108, 160, 26, GuiLongitude);
        createGuiControl(window, "STATIC", "KP longitude (0-360)", 0, 24, 158, 190, 20, 0);
        guiDegrees = createGuiControl(window, "EDIT", "", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,
            220, 154, 160, 26, GuiDegrees);
        createGuiControl(window, "STATIC", "Chart times must be entered in UT (GMT).", 0,
            410, 158, 340, 20, 0);

        createGuiControl(window, "BUTTON", "Calculate", BS_DEFPUSHBUTTON | WS_TABSTOP,
            24, 204, 120, 34, GuiCalculate);
        createGuiControl(window, "BUTTON", "Example", BS_PUSHBUTTON | WS_TABSTOP,
            158, 204, 100, 34, GuiExample);
        createGuiControl(window, "BUTTON", "Copy result", BS_PUSHBUTTON | WS_TABSTOP,
            272, 204, 110, 34, GuiCopy);
        createGuiControl(window, "STATIC", "Result", 0, 24, 292, 100, 20, 0);
        guiOutput = createGuiControl(window, "EDIT", "Choose a calculation and enter its values.",
            WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL | WS_TABSTOP,
            24, 322, 800, 300, GuiOutput);
        if (guiOutputFont) SendMessageA(guiOutput, WM_SETFONT, reinterpret_cast<WPARAM>(guiOutputFont), TRUE);
        return 0;
    }
    case WM_SIZE:
        layoutGui(window);
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case GuiCalculate:
            if (HIWORD(wParam) == BN_CLICKED) calculateGuiResult(window);
            return 0;
        case GuiExample:
            if (HIWORD(wParam) == BN_CLICKED) {
                writeGuiText(guiDate, "15-08-1995");
                writeGuiText(guiTime, "04:30:00");
                writeGuiText(guiLatitude, "51.5072");
                writeGuiText(guiLongitude, "-0.1276");
                writeGuiText(guiDegrees, "123.45");
            }
            return 0;
        case GuiCopy:
            if (HIWORD(wParam) == BN_CLICKED) copyGuiOutput(window);
            return 0;
        case GuiAyanamsa:
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                int selection = static_cast<int>(SendMessageA(guiAyanamsa, CB_GETCURSEL, 0, 0));
                if (selection >= 0 && selection < static_cast<int>(size(ayanamsaOptions)))
                    selectedAyanamsa = selection;
            }
            return 0;
        }
        break;
    case WM_GETMINMAXINFO: {
        auto limits = reinterpret_cast<MINMAXINFO*>(lParam);
        limits->ptMinTrackSize.x = 760;
        limits->ptMinTrackSize.y = 560;
        return 0;
    }
    case WM_DESTROY:
        if (guiOutputFont) DeleteObject(guiOutputFont);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(window, message, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int showCommand) {
    initializeEphemeris();
    const char* className = "AstronomialMainWindow";
    WNDCLASSA windowClass{};
    windowClass.lpfnWndProc = guiWindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = className;
    if (!RegisterClassA(&windowClass)) return 1;
    HWND window = CreateWindowExA(0, className, "Astronomial | KP Chart Calculator",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 880, 700,
        nullptr, nullptr, instance, nullptr);
    if (!window) return 1;
    ShowWindow(window, showCommand);
    UpdateWindow(window);
    MSG message{};
    while (GetMessageA(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
    return static_cast<int>(message.wParam);
}
#else
int main(int argc, char** argv) {
    initializeEphemeris();

    if (argc > 1 && string(argv[1]) == "register") return runRegistration(argc, argv);
    CoreCommands core;
    core.add("help", "Show available help", [](const CommandRequest& r){ return coreCommand(r.name, r.raw_arguments); });
    core.add("example", "Show a copyable example", [](const CommandRequest& r){ return coreCommand(r.name, r.raw_arguments); });
    core.add("about", "Explain KP astrology", [](const CommandRequest& r){ return coreCommand(r.name, r.raw_arguments); });
    core.add("birth_date", "Generate a full KP chart", [](const CommandRequest& r){ return coreCommand(r.name, r.raw_arguments); });
    core.add("planets", "Generate the planet table", [](const CommandRequest& r){ return coreCommand(r.name, r.raw_arguments); });
    core.add("houses", "Generate the house table", [](const CommandRequest& r){ return coreCommand(r.name, r.raw_arguments); });
    core.add("lords", "Calculate KP lords for a degree", [](const CommandRequest& r){ return coreCommand(r.name, r.raw_arguments); });
    CommandRouter router; router.registerPlugin(&core);
    string mode = argc > 1 ? argv[1] : "telegram";
    if (mode == "cli") return runCli(router, argc, argv);
    if (mode == "plugin") { for (const auto& c:router.commands()) cout<<c.name<<"\t"<<c.description<<"\n"; return 0; }
    string host="127.0.0.1"; int port=8787;
    for(int i=2;i<argc;++i){string a=argv[i];if(a=="--host"&&i+1<argc)host=argv[++i];else if(a=="--port"&&i+1<argc)port=stoi(argv[++i]);}
    if (mode == "web" || mode == "app") {
        if(mode=="app") {
#ifdef _WIN32
            string cmd="start http://"+host+":"+to_string(port); system(cmd.c_str());
#elif defined(__APPLE__)
            string cmd="open http://"+host+":"+to_string(port); system(cmd.c_str());
#else
            string cmd="xdg-open http://"+host+":"+to_string(port)+" >/dev/null 2>&1 &"; system(cmd.c_str());
#endif
        }
        try { serveWeb(router, host, port); } catch(const exception& e){ cerr<<e.what()<<"\n"; return 1; }
    }
    const char* tok = getenv("ASTRONOMIAL_BOT_TOKEN");
    if (!tok || !*tok) {
        cout << "Set ASTRONOMIAL_BOT_TOKEN first.\n";
        return 1;
    }
    TgBot::Bot bot(tok);

    auto dispatchTelegram=[&](TgBot::Message::Ptr message,string name){auto raw=message->text.value_or("");auto args=telegramArguments(raw);auto r=router.dispatch({name,splitArgs(args),args,"telegram"});sendHtml(bot,message->chat->id,r.exit_code?r.error:r.output);};
    bot.getEvents().onCommand("start",[&](auto m){dispatchTelegram(m,"help");});
    for(const auto& c:router.commands()) bot.getEvents().onCommand(c.name.substr(c.name.find('.')+1),[&,name=c.name](auto m){dispatchTelegram(m,name);});
    try { TgBot::TgLongPoll longPoll(bot); cout<<"Bot is running (plugin key "<<plugin_key<<")...\n"; while(true) longPoll.start(); } catch(TgBot::TgException& e){cout<<e.what()<<endl;}
    return 0;
}
#endif