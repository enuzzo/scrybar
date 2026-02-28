#pragma once
// UI string table for display-side localization.
// Language pivot: g_wordClockLang (same key used by word clock).
// Web UI remains in English and is NOT covered here.

struct UiStrings {
    // Weather panel
    const char* windFmt;              // printf fmt with one float arg
    const char* windNa;               // wind offline placeholder
    const char* forecast3h;           // printf fmt with one string arg
    const char* forecastNow;          // printf fmt with one string arg
    const char* forecastNa;           // forecast offline placeholder
    const char* weatherOffline;       // weather data unavailable
    const char* wifiOff;              // WiFi disabled at compile time
    // RSS panel
    const char* rssOffline;           // WiFi not connected
    const char* rssFeedError;         // HTTP error fetching feed
    const char* rssSyncing;           // first fetch in progress
    const char* rssDisabled;          // RSS_ENABLED=0
    // Touch hints
    const char* touchToClose;         // init-time label on QR overlay
    const char* touchToCloseAnywhere; // runtime label when QR is shown
    const char* generatingQr;         // QR not yet ready
};

// ---------------------------------------------------------------------------
// Italian (default)
// ---------------------------------------------------------------------------
static const UiStrings kUiLang_it = {
    /* windFmt              */ "Vento %.0f km/h",
    /* windNa               */ "Vento -- km/h",
    /* forecast3h           */ "Tra 3h: %s",
    /* forecastNow          */ "Ora: %s",
    /* forecastNa           */ "Tra 3h: --",
    /* weatherOffline       */ "Meteo offline",
    /* wifiOff              */ "WiFi off",
    /* rssOffline           */ "RSS offline.\nConnettiti al WiFi\ne riprova.",
    /* rssFeedError         */ "Feed non disponibile.\nRiprovo automaticamente\ntra poco.",
    /* rssSyncing           */ "Sincronizzo il feed RSS...\nAttendi qualche secondo.\n",
    /* rssDisabled          */ "RSS disabilitato in config.",
    /* touchToClose         */ "Tocca per chiudere",
    /* touchToCloseAnywhere */ "Tocca ovunque per chiudere",
    /* generatingQr         */ "Genero QR...",
};

// ---------------------------------------------------------------------------
// English
// ---------------------------------------------------------------------------
static const UiStrings kUiLang_en = {
    /* windFmt              */ "Wind %.0f km/h",
    /* windNa               */ "Wind -- km/h",
    /* forecast3h           */ "In 3h: %s",
    /* forecastNow          */ "Now: %s",
    /* forecastNa           */ "In 3h: --",
    /* weatherOffline       */ "Weather offline",
    /* wifiOff              */ "WiFi off",
    /* rssOffline           */ "RSS offline.\nConnect to WiFi\nand retry.",
    /* rssFeedError         */ "Feed unavailable.\nRetrying automatically\nsoon.",
    /* rssSyncing           */ "Syncing RSS feed...\nPlease wait a moment.\n",
    /* rssDisabled          */ "RSS disabled in config.",
    /* touchToClose         */ "Tap to close",
    /* touchToCloseAnywhere */ "Tap anywhere to close",
    /* generatingQr         */ "Generating QR...",
};

// ---------------------------------------------------------------------------
// French
// ---------------------------------------------------------------------------
static const UiStrings kUiLang_fr = {
    /* windFmt              */ "Vent %.0f km/h",
    /* windNa               */ "Vent -- km/h",
    /* forecast3h           */ "Dans 3h: %s",
    /* forecastNow          */ "Maintenant: %s",
    /* forecastNa           */ "Dans 3h: --",
    /* weatherOffline       */ "Meteo hors ligne",
    /* wifiOff              */ "WiFi off",
    /* rssOffline           */ "RSS hors ligne.\nConnectez-vous au WiFi\net reessayez.",
    /* rssFeedError         */ "Flux indisponible.\nNouvelle tentative\nautomatique bientot.",
    /* rssSyncing           */ "Synchronisation RSS...\nPatientez un instant.\n",
    /* rssDisabled          */ "RSS desactive en config.",
    /* touchToClose         */ "Touchez pour fermer",
    /* touchToCloseAnywhere */ "Touchez n'importe ou pour fermer",
    /* generatingQr         */ "Generation QR...",
};

// ---------------------------------------------------------------------------
// German
// ---------------------------------------------------------------------------
static const UiStrings kUiLang_de = {
    /* windFmt              */ "Wind %.0f km/h",
    /* windNa               */ "Wind -- km/h",
    /* forecast3h           */ "In 3h: %s",
    /* forecastNow          */ "Jetzt: %s",
    /* forecastNa           */ "In 3h: --",
    /* weatherOffline       */ "Wetter offline",
    /* wifiOff              */ "WiFi aus",
    /* rssOffline           */ "RSS offline.\nMit WiFi verbinden\nund erneut versuchen.",
    /* rssFeedError         */ "Feed nicht verfuegbar.\nAutomatischer\nNeuversuch bald.",
    /* rssSyncing           */ "RSS-Feed wird sync...\nBitte warten.\n",
    /* rssDisabled          */ "RSS in config deaktiviert.",
    /* touchToClose         */ "Tippen zum Schliessen",
    /* touchToCloseAnywhere */ "Irgendwo tippen zum Schliessen",
    /* generatingQr         */ "QR wird erstellt...",
};

// ---------------------------------------------------------------------------
// Spanish
// ---------------------------------------------------------------------------
static const UiStrings kUiLang_es = {
    /* windFmt              */ "Viento %.0f km/h",
    /* windNa               */ "Viento -- km/h",
    /* forecast3h           */ "En 3h: %s",
    /* forecastNow          */ "Ahora: %s",
    /* forecastNa           */ "En 3h: --",
    /* weatherOffline       */ "Tiempo sin conexion",
    /* wifiOff              */ "WiFi off",
    /* rssOffline           */ "RSS sin conexion.\nConectate al WiFi\ny reintenta.",
    /* rssFeedError         */ "Feed no disponible.\nReintento automatico\nen breve.",
    /* rssSyncing           */ "Sincronizando RSS...\nEspera un momento.\n",
    /* rssDisabled          */ "RSS desactivado en config.",
    /* touchToClose         */ "Toca para cerrar",
    /* touchToCloseAnywhere */ "Toca en cualquier lugar para cerrar",
    /* generatingQr         */ "Generando QR...",
};

// ---------------------------------------------------------------------------
// Portuguese
// ---------------------------------------------------------------------------
static const UiStrings kUiLang_pt = {
    /* windFmt              */ "Vento %.0f km/h",
    /* windNa               */ "Vento -- km/h",
    /* forecast3h           */ "Em 3h: %s",
    /* forecastNow          */ "Agora: %s",
    /* forecastNa           */ "Em 3h: --",
    /* weatherOffline       */ "Tempo offline",
    /* wifiOff              */ "WiFi off",
    /* rssOffline           */ "RSS offline.\nConecte ao WiFi\ne tente novamente.",
    /* rssFeedError         */ "Feed indisponivel.\nTentativa automatica\nem breve.",
    /* rssSyncing           */ "Sincronizando RSS...\nAguarde um momento.\n",
    /* rssDisabled          */ "RSS desativado na config.",
    /* touchToClose         */ "Toque para fechar",
    /* touchToCloseAnywhere */ "Toque em qualquer lugar para fechar",
    /* generatingQr         */ "Gerando QR...",
};

// ---------------------------------------------------------------------------
// Latin — divertente!
// ---------------------------------------------------------------------------
static const UiStrings kUiLang_la = {
    /* windFmt              */ "Ventus %.0f km/h",
    /* windNa               */ "Ventus -- km/h",
    /* forecast3h           */ "Post 3h: %s",
    /* forecastNow          */ "Nunc: %s",
    /* forecastNa           */ "Post 3h: --",
    /* weatherOffline       */ "Caelum absens",
    /* wifiOff              */ "WiFi absens",
    /* rssOffline           */ "RSS non adest.\nRete converte\net itera.",
    /* rssFeedError         */ "Fons non praesto.\nMox iterabitur\nautomate.",
    /* rssSyncing           */ "RSS colligitur...\nExpecta parumper.\n",
    /* rssDisabled          */ "RSS in config remotus.",
    /* touchToClose         */ "Tange ut claudas",
    /* touchToCloseAnywhere */ "Tange alicubi ut claudas",
    /* generatingQr         */ "QR formatur...",
};

// ---------------------------------------------------------------------------
// Esperanto
// ---------------------------------------------------------------------------
static const UiStrings kUiLang_eo = {
    /* windFmt              */ "Vento %.0f km/h",
    /* windNa               */ "Vento -- km/h",
    /* forecast3h           */ "Post 3h: %s",
    /* forecastNow          */ "Nun: %s",
    /* forecastNa           */ "Post 3h: --",
    /* weatherOffline       */ "Vetero eksterrete",
    /* wifiOff              */ "WiFi malaktiva",
    /* rssOffline           */ "RSS eksterrete.\nKonektu al WiFi\nkaj reprovu.",
    /* rssFeedError         */ "Fluo ne disponebla.\nAutomata reprovo\nbaldau.",
    /* rssSyncing           */ "Sinkronigado de RSS...\nBonvolu atendi.\n",
    /* rssDisabled          */ "RSS malaktivita en config.",
    /* touchToClose         */ "Tusu por fermi",
    /* touchToCloseAnywhere */ "Tusu ie ajn por fermi",
    /* generatingQr         */ "QR generata...",
};

// ---------------------------------------------------------------------------
// Neapolitan (nap)
// ---------------------------------------------------------------------------
static const UiStrings kUiLang_nap = {
    /* windFmt              */ "Viento %.0f km/h",
    /* windNa               */ "Viento -- km/h",
    /* forecast3h           */ "Fra 3h: %s",
    /* forecastNow          */ "Mo: %s",
    /* forecastNa           */ "Fra 3h: --",
    /* weatherOffline       */ "Tiempo offline",
    /* wifiOff              */ "WiFi spento",
    /* rssOffline           */ "RSS offline.\nAttaccate o WiFi\ne riprova.",
    /* rssFeedError         */ "Feed nun disp.\nRiprovo auto'\ntra poco.",
    /* rssSyncing           */ "Sincronizz' RSS...\nAspetta nu poco.\n",
    /* rssDisabled          */ "RSS disabilitato.",
    /* touchToClose         */ "Tocca pe' chiudere",
    /* touchToCloseAnywhere */ "Tocca ovunque pe' chiudere",
    /* generatingQr         */ "Genero QR...",
};

// ---------------------------------------------------------------------------
// Klingon (tlh) — ASCII transliteration, no pIqaD font needed
// ---------------------------------------------------------------------------
static const UiStrings kUiLang_tlh = {
    /* windFmt              */ "SuS %.0f km/rep",
    /* windNa               */ "SuS -- km/rep",
    /* forecast3h           */ "qen 3rep: %s",
    /* forecastNow          */ "DaH: %s",
    /* forecastNa           */ "qen 3rep: --",
    /* weatherOffline       */ "muD Qaw'",
    /* wifiOff              */ "WiFi Qaw'",
    /* rssOffline           */ "RSS Qaw'.\nWIFI yIlo'.\ntugh yIlI'.",
    /* rssFeedError         */ "RSS pagh.\nautomatically\ntugh.",
    /* rssSyncing           */ "RSS ngeHtaH...\nghorgh yIloS.\n",
    /* rssDisabled          */ "RSS QaptaHbe'.",
    /* touchToClose         */ "mevmeH yI'uch",
    /* touchToCloseAnywhere */ "Daq yI'uch nap",
    /* generatingQr         */ "QR chenmoH...",
};
