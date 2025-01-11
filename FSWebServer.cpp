#include "FSWebServer.h"
#include "sdControl.h"
#include <SPI.h>
#include <SD.h>
#include <StreamString.h>
#include "serial.h"
#include "network.h"
#include "config.h"

const char* PARAM_MESSAGE = "message";
uint8_t printer_sd_type = 0;

FSWebServer server(80);

FSWebServer::FSWebServer(uint16_t port) : AsyncWebServer(port) {}

void FSWebServer::begin(FS* fs) {
    _fs = fs;

    AsyncWebServer::begin();

    server.on("/relinquish", HTTP_GET, [this](AsyncWebServerRequest *request) {
  		this->onHttpRelinquish(request);
  	});

    server.on("/list", HTTP_GET, [this](AsyncWebServerRequest *request) {
  		this->onHttpList(request);
  	});

    server.on("/delete", HTTP_GET, [this](AsyncWebServerRequest *request) {
  		this->onHttpDelete(request);
  	});

  	server.on("/download", HTTP_GET, [this](AsyncWebServerRequest *request) {
  		this->onHttpDownload(request);
  	});

    server.on("/upload", HTTP_POST, [this](AsyncWebServerRequest *request) {
            DEBUG_LOG("upload pre 200");
            request->send(200, "text/plain", "Upload started");
        },
        [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
            DEBUG_LOG("upload pre onHttpFileUpload");
            
            if (!index) {
                if (request->hasArg("path")) {
                    String path = request->arg("path");
                    DEBUG_LOG("Found path parameter from URL: %s", path.c_str());
                } else {
                    DEBUG_LOG("No path parameter found in URL");
                }
            }
            
            this->onHttpFileUpload(request, filename, index, data, len, final);
        }
    );

    server.on("/wifiap", HTTP_POST, [this](AsyncWebServerRequest *request) {
  		this->onHttpWifiAP(request);
  	});

    server.on("/wificonnect", HTTP_POST, [this](AsyncWebServerRequest *request) {
  		this->onHttpWifiConnect(request);
  	});

    server.on("/wifistatus", HTTP_GET, [this](AsyncWebServerRequest *request) {
  		this->onHttpWifiStatus(request);
  	});
    
    server.on("/wifiscan", HTTP_GET, [this](AsyncWebServerRequest *request) {
  		this->onHttpWifiScan(request);
  	});

    server.on("/wifilist", HTTP_GET, [this](AsyncWebServerRequest *request) {
  		this->onHttpWifiList(request);
  	});

	  server.onNotFound([this](AsyncWebServerRequest *request) {
      this->onHttpNotFound(request);
    });
}

String getContentType(String filename, AsyncWebServerRequest *request) {
  if (request->hasArg("download")) return "application/octet-stream";
  else if (filename.endsWith(".htm")) return "text/html";
  else if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".json")) return "application/json";
  else if (filename.endsWith(".png")) return "image/png";
  else if (filename.endsWith(".gif")) return "image/gif";
  else if (filename.endsWith(".jpg")) return "image/jpeg";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".xml")) return "text/xml";
  else if (filename.endsWith(".pdf")) return "application/x-pdf";
  else if (filename.endsWith(".zip")) return "application/x-zip";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

void FSWebServer::onHttpWifiAP(AsyncWebServerRequest *request) {
  Serial.println("onHttpWifiAP");
  if(network.isSTAmode()) {
    request->send(200, "text/plain", "WIFI:StartAPmode");
    network.startSoftAP();
  }
  else {
    request->send(200, "text/plain", "WIFI:AlreadyAPmode");
  }
}

void FSWebServer::onHttpWifiList(AsyncWebServerRequest *request) {
  String resp;
  network.getWiFiList(resp);
  request->send(200, "text/plain", resp);
}

void FSWebServer::onHttpWifiStatus(AsyncWebServerRequest *request) {
  DEBUG_LOG("onHttpWifiStatus\n");

  String resp = "WIFI:";
  switch(network.status()) {
    case 1:
      resp += "Failed";
    break;
    case 2:
      resp += "Connecting";
    break;
    case 3:
      IPAddress ip = WiFi.localIP();
      resp += "Connected:";
      for (int i=0; i<4; i++)
        resp += i  ? "." + String(ip[i]) : String(ip[i]);
    break;
  }
  request->send(200, "text/plain", resp);
}

void FSWebServer::onHttpWifiConnect(AsyncWebServerRequest *request)
{
  String wifi_ssid,wifi_psd;
  if (request->hasArg("ssid"))
  {
    Serial.print("got ssid:");
    wifi_ssid = request->arg("ssid");
 
    Serial.println(wifi_ssid);
  } 
  else
  { 
    Serial.println("error, not found ssid");
    request->send(200, "text/plain", "WIFI:NoSSID");
    return;
  }

  if (request->hasArg("password")) 
  {
    Serial.print("got password:");
    wifi_psd = request->arg("password");
    Serial.println(wifi_psd);
  } 
  else 
  {
    Serial.println("error, not found password");
    request->send(200, "text/plain", "WIFI:NoPassword");
    return;
  }

  if(0==wifi_ssid.length() || 0==wifi_psd.length()) {
     request->send(200, "text/plain", "WIFI:WrongPara");
     return;
  }

  if(network.startConnect(wifi_ssid, wifi_psd)) {
    request->send(200, "text/plain", "WIFI:Starting");
  }
  else {
    String resp = "WIFI:";
    IPAddress ip = WiFi.localIP();
      resp += "AlreadyCon:";
      for (int i=0; i<4; i++)
        resp += i  ? "." + String(ip[i]) : String(ip[i]);
    request->send(200, "text/plain", resp);
  }

  return;
}

void FSWebServer::onHttpWifiScan(AsyncWebServerRequest * request) {
    network.doScan();
    request->send(200, "text/json", "ok");
    return;
}

bool FSWebServer::onHttpNotFound(AsyncWebServerRequest *request) {
  String path = request->url();
	DEBUG_LOG("handleFileRead: %s\r\n", path.c_str());

	if (path.endsWith("/"))
		path += "index.htm";

	String contentType = getContentType(path, request);
	String pathWithGz = path + ".gz";
	if (_fs->exists(pathWithGz) || _fs->exists(path)) {
		if (_fs->exists(pathWithGz)) {
			path += ".gz";
		}
		DEBUG_LOG("Content type: %s\r\n", contentType.c_str());
		AsyncWebServerResponse *response = request->beginResponse(*_fs, path, contentType);
		if (path.endsWith(".gz"))
			response->addHeader("Content-Encoding", "gzip");
		DEBUG_LOG("File %s exist\r\n", path.c_str());
		request->send(response);
		DEBUG_LOG("File %s Sent\r\n", path.c_str());

		return true;
	}
	else
		DEBUG_LOG("Cannot find %s\n", path.c_str());
	return false;
}

bool FSWebServer::handleFileReadSD(String path, AsyncWebServerRequest *request) {
	DEBUG_LOG("handleFileReadSD: %s\r\n", path.c_str());

	if (path.endsWith("/"))
		path += "index.htm";

	String contentType = getContentType(path, request);
	String pathWithGz = path + ".gz";
	sdcontrol.takeControl();
	if (SD.exists(pathWithGz) || SD.exists(path)) {
		if (SD.exists(pathWithGz)) {
			path += ".gz";
		}
		DEBUG_LOG("Content type: %s\r\n", contentType.c_str());
		AsyncWebServerResponse *response = request->beginResponse(SD, path, contentType);
		if (path.endsWith(".gz"))
			response->addHeader("Content-Encoding", "gzip");
		DEBUG_LOG("File %s exist\r\n", path.c_str());
		request->send(response);
		DEBUG_LOG("File %s Sent\r\n", path.c_str());

		return true;
	}
	else
		DEBUG_LOG("Cannot find %s\n", path.c_str());
	sdcontrol.relinquishControl();
	return false;
}

void FSWebServer::onHttpRelinquish(AsyncWebServerRequest *request) {
    sdcontrol.relinquishControl();
    request->send(200, "text/plain", "ok");
}

void FSWebServer::onHttpDownload(AsyncWebServerRequest *request) {
    DEBUG_LOG("onHttpDownload");

    switch(sdcontrol.canWeTakeControl())
    { 
      case -1: {
        DEBUG_LOG("Printer controlling the SD card"); 
        request->send(500, "text/plain","DOWNLOAD:SDBUSY");
      }
      return;
    
      default: break;
    }
  
    int params = request->params();
    if (params == 0) {
      DEBUG_LOG("No params");
      request->send(500, "text/plain","DOWNLOAD:BADARGS");
      return;
    }
    const AsyncWebParameter* p = request->getParam(static_cast<size_t>(0));
    String path = p->value();

    AsyncWebServerResponse *response = request->beginResponse(200);
    response->addHeader("Connection", "close");
    response->addHeader("Access-Control-Allow-Origin", "*");
    if (!this->handleFileReadSD(path, request))
      request->send(404, "text/plain", "DOWNLOAD:FileNotFound");
    delete response;
}

void FSWebServer::onHttpList(AsyncWebServerRequest * request) {
    switch(sdcontrol.canWeTakeControl()) { 
        case -1:
            DEBUG_LOG("Printer controlling the SD card\n"); 
            request->send(500, "text/plain", "LIST:SDBUSY");
            return;
        default: break;
    }

    if (request->params() == 0) {
        request->send(500, "text/plain", "LIST:BADARGS");
        return;
    }

    const AsyncWebParameter* p = request->getParam(static_cast<size_t>(0));
    String path = p->value();

    sdcontrol.takeControl();

    if (!SD.exists((char *)path.c_str())) {
        request->send(500, "text/plain", "LIST:BADPATH");
        sdcontrol.relinquishControl();
        return;
    }

    File dir = SD.open((char *)path.c_str());
    if (!dir.isDirectory()) {
        dir.close();
        request->send(500, "text/plain", "LIST:NOTDIR");
        sdcontrol.relinquishControl();
        return;
    }

    dir.rewindDirectory();
    String output = "[";
    for (int cnt = 0; true; ++cnt) {
        File entry = dir.openNextFile();
        if (!entry) {
            break;
        }
        if (cnt > 0) {
            output += ',';
        }
        output += "{\"type\":\"";
        output += (entry.isDirectory()) ? "dir" : "file";
        output += "\",\"name\":\"";
        output += entry.name();
        output += "\",\"size\":\"";
        output += String(entry.size());
        output += "\"}";
        entry.close();
    }
    output += "]";
    
    request->send(200, "application/json", output);
    
    dir.close();
    sdcontrol.relinquishControl();
}

void FSWebServer::onHttpDelete(AsyncWebServerRequest *request) {
  switch(sdcontrol.canWeTakeControl())
  { 
    case -1: {
      DEBUG_LOG("Printer controlling the SD card"); 
      request->send(500, "text/plain","DELETE:SDBUSY");
    }
    return;
  
    default: break;
  }

  Serial.println("onHttpDelete");
  if (!request->hasArg("path")) {
    request->send(500, "text/plain", "DELETE:BADARGS");
    Serial.println("no path arg");
  } 
  else {
    const AsyncWebParameter* p = request->getParam(static_cast<size_t>(0));
    String path = "/"+p->value();
    Serial.print("path:");
    Serial.println(path);

    sdcontrol.takeControl();
    if (path == "/" || !SD.exists((char *)path.c_str())) {
      request->send(500, "text/plain", "DELETE:BADPATH");
      Serial.println("path not exists");
    }
    else {
      sdcontrol.deleteFile(path);
      Serial.println("send ok");
      request->send(200, "text/plain", "ok");
    }
    sdcontrol.relinquishControl();
  }
}

void FSWebServer::onHttpFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    static File uploadFile;
    static String uploadPath;
    static bool pathInitialized = false;

    if (request->url() != "/upload") {
        DEBUG_LOG("Upload bad args"); 
        request->send(500, "text/plain", "UPLOAD:BADARGS");
        return;
    }

    if (!index) {
        sdcontrol.takeControl();
        if (uploadFile) {
            uploadFile.close();
        }

        pathInitialized = false;

        int params = request->params();
        for(int i = 0; i < params; i++) {
            const AsyncWebParameter* param = request->getParam(i);
            if(param->isPost() && param->name() == "path") {
                uploadPath = param->value();
                pathInitialized = true;
                DEBUG_LOG("Found path in multipart form data: %s\n", uploadPath.c_str());
                break;
            }
        }

        if (!pathInitialized) {
            if (request->hasArg("path")) {
                uploadPath = request->arg("path");
                pathInitialized = true;
                DEBUG_LOG("Found path in form field: %s\n", uploadPath.c_str());
            } else {
                uploadPath = "/";
                DEBUG_LOG("No path specified, using root directory\n");
            }
        }

        if (uploadPath.length() > 0) {
            if (!uploadPath.startsWith("/")) {
                uploadPath = "/" + uploadPath;
            }
            if (!uploadPath.endsWith("/")) {
                uploadPath += "/";
            }
        }

        DEBUG_LOG("Using upload path: %s\n", uploadPath.c_str());

        if (uploadPath != "/") {
            String dirPath = uploadPath;
            if (dirPath.endsWith("/")) {
                dirPath = dirPath.substring(0, dirPath.length() - 1);
            }
            
            DEBUG_LOG("Attempting to create directory: %s\n", dirPath.c_str());
            if (!SD.exists(dirPath.c_str())) {
                if (!SD.mkdir(dirPath.c_str())) {
                    DEBUG_LOG("Failed to create directory: %s\n", dirPath.c_str());
                    request->send(500, "text/plain", "UPLOAD:MKDIRFAILED");
                    sdcontrol.relinquishControl();
                    return;
                }
                DEBUG_LOG("Directory created successfully\n");
            } else {
                DEBUG_LOG("Directory already exists\n");
            }
        }

        String fullPath = uploadPath + filename;
        DEBUG_LOG("Final upload path: %s\n", fullPath.c_str());

        if (SD.exists((char *)fullPath.c_str())) {
            DEBUG_LOG("Removing existing file\n");
            SD.remove((char *)fullPath.c_str());
        }

        uploadFile = SD.open(fullPath.c_str(), FILE_WRITE);
        if (!uploadFile) {
            request->send(500, "text/plain", "UPLOAD:OPENFAILED");
            sdcontrol.relinquishControl();
            DEBUG_LOG("Upload: Open file failed: %s\n", fullPath.c_str());
        } else {
            DEBUG_LOG("Upload: File opened successfully: %s\n", fullPath.c_str());
        }
    }

    if (len) {
        if (uploadFile) {
            if (len != uploadFile.write(data, len)) {
                DEBUG_LOG("Upload: write error\n");  
            }
            DEBUG_LOG("Upload: written: %d bytes\n", len);
        }
    }

    if (final) {
        if (uploadFile) {
            uploadFile.close();
        }
        DEBUG_LOG("Upload End\n");
        sdcontrol.relinquishControl();
    }
}
