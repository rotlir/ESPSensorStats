#include <iostream>
#include <thread>
#include <chrono>
#include <cstdio>
#include <fstream>
#include "libs/HTTPRequest.hpp"
#include "libs/json.hpp"

using namespace std;
using namespace std::this_thread;
using namespace std::chrono;
using json = nlohmann::json;

struct Sensors { //structure containing names of the sensors we need to get data from
    string first_level[10];
    string second_level[10][20];
    string third_level[10][20][40];
};

Sensors sensors;
bool cut_fractal = true;
int timeout = 1;
string address;

json setup() {
    ifstream f("config.json");
    json j = json::parse(f);

    int i = 0, max_i;
    for (json::iterator it = j["data-to-include"].begin(); it != j["data-to-include"].end(); ++it) { //filling the first level
        sensors.first_level[i] = it.key();
        max_i = i;
        i++;
    };   
    

    int i_sl = 0, max_i_sl = 0;
    for (i = 0; sensors.first_level[i] != ""; i++) { //filling the second level 
        i_sl = 0;
        for (json::iterator it = j["data-to-include"][sensors.first_level[i]].begin(); it != j["data-to-include"][sensors.first_level[i]].end(); ++it){
            sensors.second_level[i][i_sl] = it.key();
            i_sl++;
            if(i_sl > max_i_sl) max_i_sl++;
        }};
    max_i_sl--;

    int i_tl = 0;
    i_sl = 0;
    for (i = 0; i <= max_i & i_sl <= max_i_sl;) { //filling the third level
        for (json::iterator it = j["data-to-include"][sensors.first_level[i]][sensors.second_level[i][i_sl]].begin();
        it != j["data-to-include"][sensors.first_level[i]][sensors.second_level[i][i_sl]].end();
        ++it) {
            sensors.third_level[i][i_sl][i_tl] = it.key();
            i_tl++;
    }
        i_sl++;
        i_tl = 0;
        if (sensors.second_level[i][i_sl] == "") {
            i++;
            i_sl = 0;
        }
    };

    //writing settings to variables
    timeout = j["settings"]["timeout"];
    cut_fractal = j["settings"]["cut_fractal"];
    address = j["settings"]["address"];

    return j;
}

string getCmdOut(string cmd){  //function to get output of shell commands 
    
    string data;
    const int max_buf = 256;
    char buf[max_buf];
    FILE *stream;

    stream = popen(cmd.c_str(), "r");
    if(stream){
        while(!feof(stream)){
            if(fgets(buf, max_buf, stream) != NULL) data.append(buf);
        }
        pclose(stream);
    }
    return data;
}

bool checkIfExists(json j, int fl, int sl, int tl) {
    if (j.contains(sensors.first_level[fl])) {
        j = j[sensors.first_level[fl]];
        if (j.contains(sensors.second_level[fl][sl])) {
            j = j[sensors.second_level[fl][sl]];
            if (j.contains(sensors.third_level[fl][sl][tl])) return true;
        }
    }
    return false;
}

int main(){ 
    int fl = 0, sl = 0, tl = 0;
    json data;
    json config = setup();

    while (true){
        int counter = 0;
        system("clear");
        data = json::parse(getCmdOut("sensors -Aj 2>&1")); 
        json to_be_sent; //data that will be sent to client
        while (true) {
            json j = config["data-to-include"][sensors.first_level[fl]][sensors.second_level[fl][sl]][sensors.third_level[fl][sl][tl]];
            json curData = data[sensors.first_level[fl]][sensors.second_level[fl][sl]][sensors.third_level[fl][sl][tl]];  
            string key;  
            float val;
            if (!j.contains("include") or j["include"]) { //if data from the sensor must be included

                if(j.contains("alias")) {
                    key = j["alias"]; //key that will be added to the to_be_sent object
                    cout<< key << " ";        
                }
                else { //if config doesn't contain an alias for the key, use key name instead
                    key = sensors.third_level[fl][sl][tl];
                    cout<<key<<" ";
                }

                val = curData;
                if (cut_fractal) { //if fractal part of the value must be cut
                    cout<<static_cast<int>(val);
                    to_be_sent[key]["value"] = static_cast<int>(val);
                }
                else {
                    cout<<val;
                    to_be_sent[key]["value"] = val;
                }

                if(j.contains("unit")) { //if key contains unit
                    cout << " " << static_cast<string>(j["unit"]);
                    to_be_sent[key]["unit"] = j["unit"];
                }
                cout<<endl;
            }
            tl++;
            if (!checkIfExists(data, fl, sl, tl)) {
                sl++;
                tl = 0;
                if (!checkIfExists(data, fl, sl, tl)) {
                    fl++;
                    sl = 0;
                    if (!checkIfExists(data, fl, sl, tl)) {
                        fl = 0;
                        break;
                    }
                }
            }
        }  
        try {
            http::Request request(address);
            request.send("POST", to_be_sent.dump());   
        }
        catch (exception e) {
            cerr<<"error"<<endl;
        }
        sleep_for(seconds(timeout));
    }
    return 0;
}