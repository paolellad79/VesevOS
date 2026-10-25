// VesevOS Recovery - accesso: amministratore di VesevOS (sfida + HMAC come la pagina del firmware) oppure, se non ce n'e nessuno,
// il codice a 8 cifre mostrato sulla seriale. Dopo 5 errori il blocco dura 60 s. Il token vale 10 minuti.
#pragma once
#include <string>

void recAuthNewCode();                 // nuovo codice a 8 cifre
const std::string& recAuthCode();

// Risposte come JSON {ok:..}. status = codice HTTP da usare.
struct RecReply { int status; std::string json; };

RecReply recAuthVer();                                        // /api/ver
RecReply recAuthStart(const std::string& user);               // /api/start
RecReply recAuthLogin(const std::string& form);               // /api/login (corpo del modulo)
bool recAuthed(const std::string& token);                     // il token e quello valido e non scaduto?
std::string recJsonErr(const std::string& msg);               // {"ok":false,"err":"..."}
