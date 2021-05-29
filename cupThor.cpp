#include <algorithm>
#include <pistache/net.h>
#include <pistache/http.h>
#include <pistache/peer.h>
#include <pistache/http_headers.h>
#include <pistache/cookie.h>
#include <pistache/router.h>
#include <pistache/endpoint.h>
#include <pistache/common.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <chrono>
#include <vector>
#include <fstream>
#include <iterator>
#include <random>
#include <regex>
#include <thread>

using namespace std;
using namespace Pistache;

// General advice: pay atetntion to the namespaces that you use in various contexts. Could prevent headaches.

// This is just a helper function to preety-print the Cookies that one of the enpoints shall receive.
void printCookies(const Http::Request& req) {
    auto cookies = req.cookies();
    std::cout << "Cookies: [" << std::endl;
    const std::string indent(4, ' ');
    for (const auto& c: cookies) {
        std::cout << indent << c.name << " = " << c.value << std::endl;
    }
    std::cout << "]" << std::endl;
}

// Some generic namespace, with a simple function we could use to test the creation of the endpoints.
namespace Generic {

    void handleReady(const Rest::Request&, Http::ResponseWriter response) {
        response.send(Http::Code::Ok, "1");
    }

}

// Definition of the OvenEnpoint class 
class CupThorEndpoint {
public:
    explicit CupThorEndpoint(Address addr)
        : httpEndpoint(std::make_shared<Http::Endpoint>(addr))
    { }

    // Initialization of the server. Additional options can be provided here
    void init(size_t thr = 2) {
        auto opts = Http::Endpoint::options()
            .threads(static_cast<int>(thr));
        httpEndpoint->init(opts);
        // Server routes are loaded up
        setupRoutes();
    }

    // Server is started threaded.  
    void start() {
        httpEndpoint->setHandler(router.handler());
        httpEndpoint->serveThreaded();
    }

    // When signaled server shuts down
    void stop(){
        httpEndpoint->shutdown();
    }

private:
    void setupRoutes() {
        using namespace Rest;
        // Defining various endpoints
        // Generally say that when http://localhost:9080/ready is called, the handleReady function should be called. 
        Routes::Get(router, "/ready", Routes::bind(&Generic::handleReady));
        Routes::Get(router, "/auth", Routes::bind(&CupThorEndpoint::doAuth, this));
        Routes::Post(router, "/settings/:settingName/:value", Routes::bind(&CupThorEndpoint::setSetting, this));
        Routes::Get(router, "/settings/:settingName/", Routes::bind(&CupThorEndpoint::getSetting, this));

        //Nu ar trebui sa se sa seteze senzorii
        //Routes::Post(router, "/sensors/:sensorName/:value", Routes::bind(&CupThorEndpoint::setSensor, this));
        Routes::Get(router, "/sensors/:sensorName/", Routes::bind(&CupThorEndpoint::getSensor, this));
                
        Routes::Post(router, "/cook/:cookName/", Routes::bind(&CupThorEndpoint::setCook, this));
        Routes::Post(router, "/cook/:cookName/:value", Routes::bind(&CupThorEndpoint::setCookMode, this));
        Routes::Get(router, "/cook/", Routes::bind(&CupThorEndpoint::getCook, this));

        


        Routes::Get(router, "/mediaplayer/", Routes::bind(&CupThorEndpoint::getMediaPlayer, this));
        Routes::Post(router, "/mediaplayer/:mediaCommandName/", Routes::bind(&CupThorEndpoint::setMediaCommand, this));
        Routes::Post(router, "/mediaplayer/:mediaCommandName/:value", Routes::bind(&CupThorEndpoint::setMediaCommandSong, this));

    }

    
    void doAuth(const Rest::Request& request, Http::ResponseWriter response) {
        // Function that prints cookies
        printCookies(request);
        // In the response object, it adds a cookie regarding the communications language.
        response.cookies()
            .add(Http::Cookie("lang", "en-US"));
        // Send the response
        response.send(Http::Code::Ok);
    }

// Endpoint to configure one of the Oven's settings.

    void setMediaCommand(const Rest::Request& request, Http::ResponseWriter response){
        // You don't know what the parameter content that you receive is, but you should
        // try to cast it to some data structure. Here, I cast the settingName to string.
        auto mediaCommandName = request.param(":mediaCommandName").as<std::string>();

        // This is a guard that prevents editing the same value by two concurent threads. 
        Guard guard(cupthorLock);


        // Setting the Oven's setting to value
        int mediaCommandResponse = cth.set_media_player_command(mediaCommandName);

        // Sending some confirmation or error response.
        if (mediaCommandResponse == 1) {
            response.send(Http::Code::Ok, "MediaPlayer is playing");
        }

        else if(mediaCommandResponse == 2){
            response.send(Http::Code::Ok, "MediaPlayer is stopping");
        }

        else if(mediaCommandResponse == 3){
            response.send(Http::Code::Ok, "Cant play in silent mode. Deactivate it first");
        }


        else {
            response.send(Http::Code::Not_Found, "Eroare media player comanda");
        }

    }




    void setMediaCommandSong(const Rest::Request& request, Http::ResponseWriter response){
        // You don't know what the parameter content that you receive is, but you should
        // try to cast it to some data structure. Here, I cast the settingName to string.
        auto mediaCommandName = request.param(":mediaCommandName").as<std::string>();


        //MediaPlayer accepta doar comanda play daca se ofera si un mp3 in Base64
        if (mediaCommandName != "play"){
            response.send(Http::Code::Ok, "The media player only accepts \"play\" if a song in Base64 is provided");
        }


        //Cazul normal
        else
        {

        // This is a guard that prevents editing the same value by two concurent threads. 
        Guard guard(cupthorLock);

        
        string val = "";
        if (request.hasParam(":value")) {
            auto value = request.param(":value");
            val = value.as<string>();
        }

        // Setting the Oven's setting to value
        int mediaCommandResponse = cth.media_player_play_given_song(mediaCommandName, val);

        // Sending some confirmation or error response.
        if (mediaCommandResponse == 1) {
            response.send(Http::Code::Ok, "Playing given song");
        }


        else if (mediaCommandResponse == 3){
            response.send(Http::Code::Ok, "Cant play in silent mode. Deactivate it first");
        }


        else {
            response.send(Http::Code::Not_Found, "An error has occured when processing the given song");
        }

        }

    }




    // Setting to get the settings value of one of the configurations of the Oven
    void getMediaPlayer(const Rest::Request& request, Http::ResponseWriter response){

        Guard guard(cupthorLock);

        string valueStatus = cth.get_media_player_status();

        if (valueStatus != "") {

            // In this response I also add a couple of headers, describing the server that sent this response, and the way the content is formatted.
            using namespace Http;
            response.headers()
                        .add<Header::Server>("pistache/0.1")
                        .add<Header::ContentType>(MIME(Text, Plain));

            response.send(Http::Code::Ok, "MediaPlayer is " + valueStatus);
        }
        else {
            response.send(Http::Code::Not_Found, "Eroare media player");
        }
    }




    // In mod normal nu ar trebui sa se intre pe aceasta sectiune de cod deoarece senzorii nu ar trebui setati ci doar interogati.
    void setCook(const Rest::Request& request, Http::ResponseWriter response){
        auto cookName = request.param(":cookName").as<std::string>();

        Guard guard(cupthorLock);
        
        int setResponse = cth.set_cook(cookName);
        if (setResponse == 1) {
            response.send(Http::Code::Ok, "Cook mode was set to " + cookName);
        }
        else if (setResponse == 2){
            response.send(Http::Code::Ok, "Silent mode is activated! \nTurn it off and try again.");
        }

        else if (setResponse == 3){
            response.send(Http::Code::Ok, "No food detected in the oven. Not starting for safety measures");
        }

        else {
            response.send(Http::Code::Not_Found, cookName + " was not a valid value ");
        }


    }
    void setCookMode(const Rest::Request& request, Http::ResponseWriter response){
        auto cookName = request.param(":cookName").as<std::string>();

        Guard guard(cupthorLock);

        string val = "";
        if (request.hasParam(":value")) {
            auto value = request.param(":value");
            val = value.as<string>();
        }


        int setResponse = cth.set_cook_mode(cookName, val);

        if (setResponse == 1){

            response.send(Http::Code::Ok, "Cook mode was set to " + cookName + " with:- keep-food-warm");

        }
        else if (setResponse == 3){
            response.send(Http::Code::Ok, "Cook mode was set to " + cookName + " without:- keep-food-warm");
        }
        else if (setResponse == 2){
            response.send(Http::Code::Ok, "Silent mode is activated! \nTurn it off and try again.");
        }

         else if (setResponse == 4){
            response.send(Http::Code::Ok, "No food detected in the oven. Not starting for safety measures");
        }

        else{

            response.send(Http::Code::Not_Found, "Error! Selected cook mode cannot be set");

        }

    }
    void getCook(const Rest::Request& request, Http::ResponseWriter response){

        Guard guard(cupthorLock);

        bool cook_mode_checker = cth.get_cook_mode_status();
        string whats_cook = cth.get_what_is_cooking();

        if (whats_cook != "") {

          
            using namespace Http;
            response.headers()
                        .add<Header::Server>("pistache/0.1")
                        .add<Header::ContentType>(MIME(Text, Plain));
            if (cook_mode_checker == true)
                response.send(Http::Code::Ok, "Currently cooking: " + whats_cook + " keep-warm-food:ON");
            else
                response.send(Http::Code::Ok, "Currently cooking: " + whats_cook + " keep-warm-food:OFF");
        }
        else {
            response.send(Http::Code::Not_Found, + "Nothing is cooking right now");
        }

    }




    void setSensor(const Rest::Request& request, Http::ResponseWriter response){
        // You don't know what the parameter content that you receive is, but you should
        // try to cast it to some data structure. Here, I cast the settingName to string.
        auto sensorName = request.param(":sensorName").as<std::string>();

        // This is a guard that prevents editing the same value by two concurent threads. 
        Guard guard(cupthorLock);

        
        string val = "";
        if (request.hasParam(":value")) {
            auto value = request.param(":value");
            val = value.as<string>();
        }

        // Setting the Oven's setting to value
        int setResponse = cth.set_sensor(sensorName, val);

        // Sending some confirmation or error response.
        if (setResponse == 1) {
            response.send(Http::Code::Ok, sensorName + " was set to " + val);
        }


        else {
            response.send(Http::Code::Not_Found, sensorName + " was not found and or '" + val + "' was not a valid value ");
        }

    }




    // Setting to get the settings value of one of the configurations of the Oven
    void getSensor(const Rest::Request& request, Http::ResponseWriter response){
        auto sensorName = request.param(":sensorName").as<std::string>();

        Guard guard(cupthorLock);

        string valueSensor = cth.get_sensor(sensorName);

        if (valueSensor != "") {

            // In this response I also add a couple of headers, describing the server that sent this response, and the way the content is formatted.
            using namespace Http;
            response.headers()
                        .add<Header::Server>("pistache/0.1")
                        .add<Header::ContentType>(MIME(Text, Plain));

            response.send(Http::Code::Ok, sensorName + " is " + valueSensor);
        }
        else {
            response.send(Http::Code::Not_Found, sensorName + " was not found");
        }
    }




    // Endpoint to configure one of the Oven's settings.
    void setSetting(const Rest::Request& request, Http::ResponseWriter response){
        // You don't know what the parameter content that you receive is, but you should
        // try to cast it to some data structure. Here, I cast the settingName to string.
        auto settingName = request.param(":settingName").as<std::string>();

        // This is a guard that prevents editing the same value by two concurent threads. 
        Guard guard(cupthorLock);

        
        string val = "";
        if (request.hasParam(":value")) {
            auto value = request.param(":value");
            val = value.as<string>();
        }

        // Setting the Oven's setting to value
        int setResponse = cth.set_setting(settingName, val);

        // Sending some confirmation or error response.
        if (setResponse == 1) {
            response.send(Http::Code::Ok, settingName + " was set to " + val);
        }
        else if(setResponse == 2){
            if (val == "true")
                response.send(Http::Code::Ok, "Silent mode is activated. \nAmbient light is turned off and ventilation is set to at most 2.");
            else 
                response.send(Http::Code::Ok, "Silent mode is deactivated. \nAmbient light is turned on and ventilation is unchanged.");

        }
        else if(setResponse == 3){
            response.send(Http::Code::Ok, "Silent mode is activated! \nTurn it off and try again.");
        }
        else {
            response.send(Http::Code::Not_Found, settingName + " was not found and or '" + val + "' was not a valid value ");
        }

    }

    // Setting to get the settings value of one of the configurations of the Oven
    void getSetting(const Rest::Request& request, Http::ResponseWriter response){
        auto settingName = request.param(":settingName").as<std::string>();

        Guard guard(cupthorLock);

        string valueSetting = cth.get_setting(settingName);

        if (valueSetting != "") {

            // In this response I also add a couple of headers, describing the server that sent this response, and the way the content is formatted.
            using namespace Http;
            response.headers()
                        .add<Header::Server>("pistache/0.1")
                        .add<Header::ContentType>(MIME(Text, Plain));

            response.send(Http::Code::Ok, settingName + " is " + valueSetting);
        }
        else {
            response.send(Http::Code::Not_Found, settingName + " was not found");
        }
    }

    // Defining the class of the Oven. It should model the entire configuration of the Oven
    class CupThor {
    public:
        explicit CupThor(){ 

        this -> silent_mode.name = "silent_mode";
        this -> silent_mode.value = false;

        this -> ventilation.name = "ventilation";
        this -> ventilation.value = 0;

        this -> ambient_light.name = "ambient_light";
        this -> ambient_light.value = false;

        this -> desired_temperature.name = "desired_temperature";
        this -> desired_temperature.value = 20;
        }


        int set_media_player_command(std::string name){

            if (name == "play"){

                if (silent_mode.value == true)
                    return 3;

                media_player.set_status(true);
                return 1;
            }


            if (name == "stop"){
                media_player.set_status(false);
                return 2;
            }

            return 0;
        }


        int media_player_play_given_song(std::string name, std::string value){

            if (name == "play"){

                if (silent_mode.value == true)
                    return 3;

                if (media_player.play(value))
                    return 1;
            }

            return 0;
        }

        // Setting the value for one of the settings. Hardcoded for the defrosting option
        int set_setting(std::string name, std::string value){
            if(name == "defrost"){
                defrost.name = name;
                if(value == "true"){
                    defrost.value = true;
                    return 1;
                }
                if(value == "false"){
                    defrost.value = false;
                    return 1;
                }
            }



            if(name == "desired_temperature"){
                desired_temperature.name = name;

                double valoare;
                valoare = std::stof(value);


                if (valoare <= 300 && valoare >= 20)
                {
                    desired_temperature.value = valoare;
                    thermostat_cupthor.modifica_temperatura_la(valoare);
                    return 1;
                }
                
            }

            if (name == "ambient_light"){
                ambient_light.name = name;

                if (value == "true"){
                    if(silent_mode.value)
                    {
                        ambient_light.value = false;
                        return 3;
                    }
                     
                    else
                    {
                    ambient_light.value = true;
                    return 1;
                    }
                }
                
                if (value == "false"){
                    ambient_light.value = false;
                    return 1;
                }
            }


            if (name == "ventilation"){
                ventilation.name = name;

                int valoare;
                valoare = std::stoi(value);

                if (valoare >= 0 && valoare <=6){
                    if (silent_mode.value && valoare > 2)
                        return 3;
                    else
                    {
                    ventilation.value = valoare;
                    return 1;
                    }
                }
            }

            if (name == "silent_mode"){
                silent_mode.name = name;

                if (value == "true"){
                    silent_mode.value = true;
                    ambient_light.value = false;
                    media_player.set_status(false);
                    if (ventilation.value > 2)
                        ventilation.value = 2;

                    return 2;
                }
                
                if (value == "false"){
                    silent_mode.value = false;
                    ambient_light.value = true;
                    return 2;
                }
            }

            return 0;
        }
        int set_cook(std::string name){
            if (cantar_cupthor.get_valoare_greutate() > 0){

                int time = 0;
                int base_time = 10;
                int chicken_time = 5;
                int vegetable_time = 2;
                int fish_time = 4;
                int pork_time = 7;

                if (name == "chicken"){

                    if (silent_mode.value == true){
                        return 2;
                        cookMode.set_status(false,"");
                    }

                    else{
                        time = base_time + (chicken_time * (cantar_cupthor.get_valoare_greutate() / 100));
                        cooking_timer.set(time, name);
                        ventilation.value = 4;
                        desired_temperature.value = 200;
                        cookMode.set_status(false,name);
                        return 1;
                    }
                }
                if (name == "vegetables"){
                    time = base_time + (vegetable_time * (cantar_cupthor.get_valoare_greutate() / 100));
                    cooking_timer.set(time, name);
                    ventilation.value = 1;
                    desired_temperature.value = 100;
                    cookMode.set_status(false,name);
                    return 1;

                }
                if (name == "fish"){
                    if (silent_mode.value == true){
                        return 2;
                        cookMode.set_status(false,"");
                    }

                    else{
                        time = base_time + (fish_time * (cantar_cupthor.get_valoare_greutate() / 100));
                        cooking_timer.set(time, name);
                        ventilation.value = 4;
                        desired_temperature.value = 250;
                        cookMode.set_status(false,name);
                        return 1;
                    }
                }
                if (name == "pork"){
                    time = base_time + (pork_time * (cantar_cupthor.get_valoare_greutate() / 100));
                    cooking_timer.set(time, name);
                    ventilation.value = 2;
                    desired_temperature.value = 120;
                    cookMode.set_status(false,name);
                    return 1;
                }
            }

            if (name == "pork" || name == "chicken" || name == "fish" || name == "vegetables")
                return 3;
            return 0;

        }
        int set_cook_mode(std::string name, std::string value){

            if (value != "true" && value != "false")
                return 0;
            
            if (cantar_cupthor.get_valoare_greutate() > 0){
                int cook_feed = set_cook(name);
                if (cook_feed == 1){
                    
                    if (value == "true"){
                    cookMode.set_status(true,name);
                    return 1;
                    }


                    else if (value == "false"){
                    cookMode.set_status(false,name);
                    return 3;
                    }
                }
                else 
                if (cook_feed == 2)
                    return 2;
                
            }

            if (name == "pork" || name == "chicken" || name == "fish" || name == "vegetables")
                return 4;


            return 0;

        }
        //SET-SENSOR - nu ar trebui implementat nimic aici
        int set_sensor(std::string name, std::string value){

            
            return 0;
        }

        // Getter
        string get_setting(std::string name){

            //SETTINGS
            if (name == "defrost"){
                return std::to_string(defrost.value);
            }


            else if (name == "desired_temperature"){
                return std::to_string(desired_temperature.value);
            }

            else if (name == "ambient_light"){
                return std::to_string(ambient_light.value);
            }

            else if (name == "ventilation"){
                return std::to_string(ventilation.value);
            }

            else if (name == "silent_mode"){
                return std::to_string(silent_mode.value);
            }

            

            else{
                return "";
            }
        }


        string get_sensor(std::string name){
            
            //SENSORS
            if (name == "thermostat"){
                return std::to_string(thermostat_cupthor.get_temperatura());
            }


            if (name == "camera"){
                return camera.get_feed();
            }

            if (name == "foodweight"){
                return std::to_string(cantar_cupthor.get_valoare_greutate());
            }

            if (name == "smoke_sensor"){
                return std::to_string(senzor_fum.get_status_senzor());
            }
            if (name == "water_jet"){
                return std::to_string(water.value);
            }

            else{
                return "";
            }

        }

        bool get_cook_mode_status(){
            return cookMode.get_status();
        }


        string get_what_is_cooking(){
            return cookMode.get_what_is_cooking();
        }
        string get_media_player_status(){
            return std::to_string(media_player.get_status());
        }


    private:
        class CookMode{
            public:
            CookMode(){
                this -> keep_food_warm = false;
                this -> what_is_cooking = "";
            }


            bool get_status(){
                return this -> keep_food_warm;
            }
            string get_what_is_cooking(){
                return this -> what_is_cooking;
            }

            void set_status(bool value, string name){
                this -> keep_food_warm = value;
                this -> what_is_cooking = name;
            }

            private:
            bool keep_food_warm;
            string what_is_cooking;
        }cookMode;

        class MediaPlayer{
            public:
                MediaPlayer(){
                    this -> status = false;
                }

                bool get_status(){
                    return this -> status;
                }


                void set_status(bool pornit_sau_oprit){
                    this -> status = pornit_sau_oprit;
                }

                bool play(std::string value){
                    std::regex expresie("^(?:[A-Za-z0-9+/]{4})*(?:[A-Za-z0-9+/]{2}==|[A-Za-z0-9+/]{3}=|[A-Za-z0-9+/]{4})$");
                    if (regex_match(value, expresie)){
                        this -> set_status(true);
                        return true;
                    }

                    return false;
                }

            private:

                bool status;

        }media_player;

        class ThermostatCupThor{
            public:

                ThermostatCupThor(){
                    this -> valoare_dorita_stored = 20;
                    this -> temperatura_la_ultima_comanda = 20;
                    this -> timpul_ultimei_comenzi = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::system_clock::now().time_since_epoch()).count();
                }

                void modifica_temperatura_la(double valoare_dorita){
                    this -> temperatura_la_ultima_comanda = this -> get_temperatura();
                    this -> timpul_ultimei_comenzi = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::system_clock::now().time_since_epoch()).count();
                    this -> valoare_dorita_stored = valoare_dorita;
                    return;
                }

                int get_temperatura(){

                    double timp_actual = std::chrono::duration_cast<std::chrono::duration<double>>(std::chrono::system_clock::now().time_since_epoch()).count();


                    if ((timp_actual - (this -> timpul_ultimei_comenzi)) > abs(((this -> valoare_dorita_stored) - (this -> temperatura_la_ultima_comanda)))){

                        return this -> valoare_dorita_stored;
                    }

                    else
                    {
                        if (valoare_dorita_stored > temperatura_la_ultima_comanda)
                            return (this -> temperatura_la_ultima_comanda) + (timp_actual - (this -> timpul_ultimei_comenzi));
                        
                        else
                            return (this -> temperatura_la_ultima_comanda) - (timp_actual - (this -> timpul_ultimei_comenzi));
                    }

                }

            private:
                double valoare_dorita_stored;
                double temperatura_la_ultima_comanda;
                double timpul_ultimei_comenzi;

        }thermostat_cupthor;

        //Simulare camera cupthor
        class Camera{
            public:
                Camera(){

                }

                std::string get_feed(){
                        
                        struct pixel{
                                unsigned char r;
                                unsigned char g;
                                unsigned char b;
                            };

                        struct{
                            std::vector<unsigned char> header;
                            std::vector<pixel> pixeli;
                        }imagine;

                        std::ifstream input("./CameraFakeInput/peppers.bmp", std::ios::binary);
                        std::ofstream output("./OutputCamera/picture.bmp", std::ios::binary);
                        
                        std::vector<unsigned char> buffer (std::istreambuf_iterator<char>(input), {});
                        
                        //simulez output-uri diferite

                        int variante = rand()%11;

                        if (variante != 0)
                        {
                            //hardcodat pentru imaginea noastra
                            int trio = 0;
                            pixel pixi;
                            for (int i = 0 ; i<= buffer.size(); i++)
                            {   
                                if (i <= 54)
                                    imagine.header.push_back(buffer[i]);
                                
                                else if (trio == 0)
                                {
                                    pixi.b = buffer[i];
                                    trio++;
                                }
                                else if (trio == 1)
                                {
                                    pixi.g = buffer[i];
                                    trio++;
                                }
                                else if (trio == 2)
                                {
                                    pixi.r = buffer[i];
                                    trio = 0;

                                    pixel aux;
                                    aux.r = pixi.r;
                                    aux.g = pixi.g;
                                    aux.b = pixi.b;

                                    imagine.pixeli.push_back(aux);
                                }

                            }

                            for (int i=0; i<= imagine.header.size(); i++)
                            {
                                output << imagine.header[i];
                            }

                            for (int i=0; i<= imagine.pixeli.size(); i++)
                            {
                                if (variante == 1){
                                output << imagine.pixeli[i].g;
                                output << imagine.pixeli[i].g;
                                output << imagine.pixeli[i].b;
                                }
                                else if (variante == 2){
                                output << imagine.pixeli[i].r;
                                output << imagine.pixeli[i].g;
                                output << imagine.pixeli[i].g;
                                }

                                else if (variante == 3){
                                output << imagine.pixeli[i].g;
                                output << imagine.pixeli[i].g;
                                output << imagine.pixeli[i].g;
                                }

                                else if (variante == 4){
                                output << imagine.pixeli[i].r;
                                output << imagine.pixeli[i].g;
                                output << imagine.pixeli[i].b;
                                }

                                else if (variante == 5){
                                output << imagine.pixeli[i].r;
                                output << imagine.pixeli[i].r;
                                output << imagine.pixeli[i].b;
                                }

                                else if (variante == 6){
                                output << imagine.pixeli[i].r;
                                output << imagine.pixeli[i].r;
                                output << imagine.pixeli[i].r;
                                }

                                else if (variante == 7){
                                output << imagine.pixeli[i].r;
                                output << imagine.pixeli[i].g;
                                output << imagine.pixeli[i].r;
                                }

                                else if (variante == 8){
                                output << imagine.pixeli[i].b;
                                output << imagine.pixeli[i].b;
                                output << imagine.pixeli[i].b;
                                }

                                else if (variante == 9){
                                output << imagine.pixeli[i].r;
                                output << imagine.pixeli[i].b;
                                output << imagine.pixeli[i].b;
                                }

                                else if (variante == 10){
                                output << imagine.pixeli[i].b;
                                output << imagine.pixeli[i].g;
                                output << imagine.pixeli[i].b;
                                }
                            }
                        }
                        else
                        {

                        for (int i =0 ; i<= buffer.size() ; i++)
                            {
                                output << buffer[i];
                            }
                        }

                        input.close();
                        output.close();
                    return "storing photo in folder";
                        
                }

        }camera;
        // Simulare cantar
        class Cantar{
            public:

            Cantar(){
                this -> valoare_greutate = 0;
            }

            int get_valoare_greutate(){


                unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
                std::default_random_engine generator (seed);

                std::uniform_real_distribution<double> dist_unif (0.0,100.0);
                std::normal_distribution<double> dist_normal(300,100);
                double odd = dist_unif(generator);
                double computed_weight = dist_normal(generator);

                
                if (odd >= 35){
                    this -> valoare_greutate  = computed_weight;
                }  
                else
                    return 0;
                if( this -> valoare_greutate < 100 && this -> valoare_greutate!= 0)	
                    this -> valoare_greutate  = 100;
                else

                if (this -> valoare_greutate > 800)
                    this -> valoare_greutate  = 800;

                return (int)valoare_greutate;

            }

            private:
                double valoare_greutate;
        }cantar_cupthor;


        class Timer{
            public:

                Timer(){
                    this -> time = 0;
                    this -> name = "";
                    this -> setted = 0;
                    this -> pinter_aux_timer = &(this -> setted);
                }

                void set(int value, std::string name_timer){
                    this -> name = name_timer;
                    this -> time = value;
                    
                    if (this -> setted == 1){
                        this -> setted = 0 ;
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        this -> setted = 1;
                    }
                    else
                        this -> setted = 1;

                    std::string path = "./Timers/" + this -> name + ".txt";
                    std::ofstream output(path);
                    output << "working";
                    output.close();


                    std::thread t(functie_aux, this -> time, this -> name, this -> pinter_aux_timer);


                    t.detach();


                }


            private:

                int *pinter_aux_timer;
                int time;
                int setted;
                std::string name;

                static void functie_aux(int time, std::string name, int *bikini){

                    while (time > 0){
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        time = time - 1;
                        if (*bikini == 0){
                            std::string path = "./Timers/" + name + ".txt";
                            std::ofstream output(path);
                            output << "done";
                            break;
                            }
                        
                        if (time <= 0)
                            {
                                std::string path = "./Timers/" + name + ".txt";
                                std::ofstream output(path);
                                output << "done";
                            }
                    }

                }
        };


        //TODO  TREBUIE APELATA SI IN CONSTRUCTORUL CUP-THORULUI CU alaram.set_alarm()
        //senzorii importanti ar fi temperatura si senzorul de fum
        class Alarma{
            public:
                void set_alarm(){
                    std::thread t(functie_aux);
                    t.detach();
                }

            private:
                static void functie_aux(){
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    std::string path = "./Alarm/firealarm.txt";
                    std::ofstream output(path);

                    while (true){
                        int senzor = 0;
                        int dummy_utilitati = 1;

                        if (senzor == 1)
                            output << "The alarm has been triggerd";
                            dummy_utilitati = 0;
                            break;
                    }
                    
                    output.close();
                    //ceva gen exit(0) gen iesire fortata sa se inchida
                }
        }alarm;


        //TODO Asta ar trebui apelat la alarma sa se vada statusul
        //Ar fi frumos sa-i faceti path-ul pt get ca la ceilalti senzori
        class SenzorFum{
            public:
                bool get_status_senzor(){
                    bool sansa;
                    int nr = rand() % 1000;
                    if(nr == 0)
                        sansa = 1;
                    else
                        sansa = 0;
                    sansa = 0 ; //Pentru motive didactice
                    return sansa;
                }
        }senzor_fum;

        // Defining and instantiating settings.
        struct boolSetting{
            std::string name;
            bool value;
        }defrost;


        struct temperatureSetting{
            std::string name;
            double value;
        }desired_temperature;


        struct ambient_lightSetting{
            std::string name;
            bool value;
        }ambient_light;


        struct ventilationSetting{
            std::string name;
            int value;
        }ventilation;


        struct silent_modeSetting{
            std::string name;
            bool value;
        }silent_mode;


        struct water_jet{
            std::string name;
            bool value;
        }water;

        Timer cooking_timer;
    };

    // Create the lock which prevents concurrent editing of the same variable
    using Lock = std::mutex;
    using Guard = std::lock_guard<Lock>;
    Lock cupthorLock;

    // Instance of the Oven model
    CupThor cth;

    // Defining the httpEndpoint and a router.
    std::shared_ptr<Http::Endpoint> httpEndpoint;
    Rest::Router router;
};

int main(int argc, char *argv[]) {

    // This code is needed for gracefull shutdown of the server when no longer needed.
    sigset_t signals;
    if (sigemptyset(&signals) != 0
            || sigaddset(&signals, SIGTERM) != 0
            || sigaddset(&signals, SIGINT) != 0
            || sigaddset(&signals, SIGHUP) != 0
            || pthread_sigmask(SIG_BLOCK, &signals, nullptr) != 0) {
        perror("install signal handler failed");
        return 1;
    }

    // Set a port on which your server to communicate
    Port port(9080);

    // Number of threads used by the server
    int thr = 2;

    if (argc >= 2) {
        port = static_cast<uint16_t>(std::stol(argv[1]));

        if (argc == 3)
            thr = std::stoi(argv[2]);
    }

    Address addr(Ipv4::any(), port);

    cout << "Cores = " << hardware_concurrency() << endl;
    cout << "Using " << thr << " threads" << endl;

    // Instance of the class that defines what the server can do.
    CupThorEndpoint stats(addr);

    // Initialize and start the server
    stats.init(thr);
    stats.start();


    // Code that waits for the shutdown sinal for the server
    int signal = 0;
    int status = sigwait(&signals, &signal);
    if (status == 0)
    {
        std::cout << "received signal " << signal << std::endl;
    }
    else
    {
        std::cerr << "sigwait returns " << status << std::endl;
    }

    stats.stop();
}