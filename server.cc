// server.cc

// TODO: Clean out all these garbage comments.

// KML for n13-romeo-08 (Seattle area Ingress cell) exported from
// http://ingress-cells.appspot.com/?q=seattle%2C%20wa
//            <LinearRing>
//              <tessellate>0</tessellate>
//              <coordinates>-122.242468,47.994058,0.0 -123.977276,47.430873,0.0 -122.693956,46.457519,0.0 -120.994610,46.984342,0.0 -122.242468,47.994058,0.0 </coordinates>
//            </LinearRing>
//
// Desired output: Google maps polyline sets for each reasonable
// cell level (Niantic didn't go to millimeters with population data).
// That would allow me to toggle the cell grids at each level to figure
// out where population boundries exist.
//
// From cell to loop:
// S2Loop(const S2Cell& cell);
//
// From cell to polygon:
// explicit S2Polygon(const S2Cell& cell);
//
// S2Polygon function to initialize to cell union:
//void InitToCellUnionBorder(const S2CellUnion& cells);
//
// Generate the cells using S2RegionCoverer
//
// The bounds I care about for Jimi:
// 47.488589, -122.174818
// 47.484891, -122.174861
// 47.485036, -122.169507
// 47.488540, -122.169306
// level 2-20, cells 10 was the sidewalk labs default
// It produced:
// 549067de3,549067dfc,549067e04,549067e0c,549067e11,549067e1201,549067e172c,549067e1b,5490680a9c,5490680ab

// Cell level is lowest numbered 1 bit (says http://s2geometry.io/devguide/s2cell_hierarchy.html)
// 549067de3 level 1, but levels 2-20???
// 549067dfc c=1100 level 3
// 549067e04 4=0100 level 3
// 549067e0c level 3
// 549067e11 level 1
// 549067e1201 level 1
// 549067e172c level 3
// 549067e1b b=1011 level 1
// 5490680a9c level 3
// 5490680ab level 1
// So that's weird

// Setting minLevel = maxLevel in the sidewalk labs demo shows us that the discrepency at Jimi requires a cell level of
// at least 12.  n13-romeo-08 is level 6.  Level 18 is really too tight a fit to be reasonable (probably).
// Level 12 is pretty large areas.  So lets do 12-18 inclusive.  And see how much data that is...
// Maybe going to 18 would be insane.

// For level 6, sidewalk labs says: 5491,549b (it gives two cells for whatever reason)
// Picking a point in Centralia, it says: 5491,5493
// So... n13-romeo-08 is 5491.
// So.. cell 5491 to polygon or loop then generate s2RegionCoverer from that polygon or loop
// Except 5491 is way bigger than I care about.  And Jimi is pretty small.
// My playbox is within:
// 47.416464, -121.874288
// 47.859055, -121.705051
// 47.661905, -122.436668
// 47.373463, -122.395961

#include <cstdio>
#include <memory.h>
#include <aws/core/Aws.h>
#include "s2/s2region_coverer.h"
#include "s2/s2region.h"
#include "s2/s2latlng.h"
#include "s2/s2latlng_rect.h"
#include "s2/s2polygon.h"

#if 0
int main(int argc, char **argv) {
    S2CellId cellId = S2CellId::FromToken("5491", 4);
    S2Cell cell(cellId); // nr13-romeo-08
    char delimeter[8];
    std::strcpy(delimeter, "");
    for (int i=0; i<4; i++) {
        S2LatLng point(cell.GetVertex(i));
        std::printf(
            "%s{\"lat\":%f,\"lng\":%f}",
            delimeter,
            point.lat().degrees(),
            point.lng().degrees()
        );
        std::strcpy(delimeter, ",");
    }
    std::printf("\n");
    
    S2LatLngRect rect;
    S2LatLng point;
    //point = S2LatLng::FromDegrees(47.416464, -121.874288).Normalized();
    //rect.AddPoint(point);
    //point = S2LatLng::FromDegrees(47.859055, -121.705051).Normalized();
    //rect.AddPoint(point);
    //point = S2LatLng::FromDegrees(47.661905, -122.436668).Normalized();
    //rect.AddPoint(point);
    //point = S2LatLng::FromDegrees(47.373463, -122.395961).Normalized();
    //rect.AddPoint(point);
    
    // Area right around Jimi in case we need much smaller cells
    point = S2LatLng::FromDegrees(47.490477, -122.178857).Normalized();
    rect.AddPoint(point);
    point = S2LatLng::FromDegrees(47.490900, -122.164250).Normalized();
    rect.AddPoint(point);
    point = S2LatLng::FromDegrees(47.483857, -122.164184).Normalized();
    rect.AddPoint(point);
    point = S2LatLng::FromDegrees(47.484339, -122.179119).Normalized();
    rect.AddPoint(point);
    
    std::FILE * outFile;
    outFile = std::fopen ("s2cells.json","w");
    std::fprintf(outFile, "{");

    char levelDelimiter[8];
    std::strcpy(levelDelimiter,"");

    S2RegionCoverer::Options options;
    options.set_max_cells(100000);
    
    for (int level=12; level<17; level++) {
        options.set_min_level(level);
        options.set_max_level(level);
        S2RegionCoverer coverer(options);
    
        std::vector<S2CellId> cells;
        coverer.GetCovering(rect, &cells);
        
        std::printf("Level %d produced %lu cells\n", level, cells.size());

        char cellDelimiter[8];
        std::strcpy(cellDelimiter,"");
        std::fprintf(outFile, "%s\"l%d\":[", levelDelimiter, level);
        std::strcpy(levelDelimiter, ",");
        for (auto id : cells) {
            std::fprintf(outFile, "%s", cellDelimiter);
            std::strcpy(cellDelimiter, ",");
            S2Cell cell(id);
            char delimeter[8];
            std::strcpy(delimeter, "");
            std::fprintf(outFile, "[");
            for (int i=0; i<4; i++) {
                S2LatLng point(cell.GetVertex(i));
                std::fprintf(
                    outFile,
                    "%s{\"lat\":%f,\"lng\":%f}",
                    delimeter,
                    point.lat().degrees(),
                    point.lng().degrees()
                );
                std::strcpy(delimeter, ",");
            }
            std::fprintf(outFile, "]");
        }
        std::fprintf(outFile, "]");
    }
    std::fprintf(outFile, "}");
    std::fclose(outFile);
    
    // Generate the level 12 cell coverage of each edge of the level 6 cell
    /*
    options.set_min_level(12);
    options.set_max_level(12);
    S2RegionCoverer coverer(options);
    outFile = std::fopen ("edges.json","w");
    std::fprintf(outFile, "{");
    char edgeDelimiter[8];
    std::strcpy(edgeDelimiter,"");
    for (int i=0; i<4; i++) {
        S2LatLngRect rect;
        rect.AddPoint(cell.GetVertex(i));
        rect.AddPoint(cell.GetVertex((i+1)%4));
    
        std::vector<S2CellId> cells;
        coverer.GetCovering(rect, &cells);

        char cellDelimiter[8];
        std::strcpy(cellDelimiter,"");
        std::fprintf(outFile, "%s\"edge%d\":[", edgeDelimiter, i);
        std::strcpy(edgeDelimiter, ",");
        
        // copy pasta
        for (auto id : cells) {
            std::fprintf(outFile, "%s", cellDelimiter);
            std::strcpy(cellDelimiter, ",");
            S2Cell cell(id);
            char delimeter[8];
            std::strcpy(delimeter, "");
            std::fprintf(outFile, "[");
            for (int i=0; i<4; i++) {
                S2LatLng point(cell.GetVertex(i));
                std::fprintf(
                    outFile,
                    "%s{\"lat\":%f,\"lng\":%f}",
                    delimeter,
                    point.lat().degrees(),
                    point.lng().degrees()
                );
                std::strcpy(delimeter, ",");
            }
            std::fprintf(outFile, "]");
        }
        std::fprintf(outFile, "]");

    }
    std::fprintf(outFile, "}");
    std::fclose(outFile);
    */

    // Metamorphosis - East Memorial Wall - Memorial Fountain
    // 47.48793,-122.173166
    // 47.488078,-122.169664
    // 47.486942,-122.171348
    // But we want left to be inside as we walk the path.  
    // So Memorial Fountatain to East Memorial Wall to Metamorphosis

    struct Field {
        double latLng[3][2];
    } fields[] = {
        // East Memorial Wall - Memorial Fountain - South Lawn Memorial
        {.latLng={{47.488078,-122.169664},{47.486942,-122.171348},{47.485165,-122.172433}}},
        //All to Myself I Think of You Memorial - Renton Aerie No 1722 F.O.E. - Sun Dial
        {.latLng={{47.486764,-122.170208},{47.488222,-122.171616},{47.486194,-122.171397}}},
        //Metamorphosis - East Memorial Wall - South Lawn Memorial
        {.latLng={{47.48793,-122.173166},{47.488078,-122.169664},{47.485165,-122.172433}}},
        //Lullabyland Memorial - Renton Aerie No 1722 F.O.E. - Love Forever Headstone
        {.latLng={{47.486297,-122.172124},{47.488222,-122.171616},{47.48752,-122.172903}}},
        //Lullabyland Memorial - Renton Aerie No 1722 F.O.E. - Sun Dial
        {.latLng={{47.486297,-122.172124},{47.488222,-122.171616},{47.486194,-122.171397}}},
        //All to Myself I Think of You Memorial - Renton Aerie No 1722 F.O.E. - Cedar View Memorial
        {.latLng={{47.486764,-122.170208},{47.488222,-122.171616},{47.488451,-122.169666}}},
        //Love Forever Headstone - Fish Fountain - Prisoner of War Memorial
        {.latLng={{47.48752,-122.172903},{47.486199,-122.173564},{47.486325,-122.172827}}},
        //Yeah Yeah Thanks Puddin - Remembered Forever - Fountain of Youth
        {.latLng={{47.488086,-122.174574},{47.487686,-122.173709},{47.486378,-122.174457}}},
        //Metamorphosis - Jimi Hendrix Memorial - South Lawn Memorial
        {.latLng={{47.48793,-122.173166},{47.486514,-122.173947},{47.485165,-122.172433}}},
        //Garden of Harmony - Love Forever Headstone - Fish Fountain
        {.latLng={{47.486712,-122.173687},{47.48752,-122.172903},{47.486199,-122.173564}}},
        //Garden of Harmony - Remembered Forever - Fountain of Youth
        {.latLng={{47.486712,-122.173687},{47.487686,-122.173709},{47.486378,-122.174457}}},
        //Iu-Mienh Community Memorial  - Fish Fountain - Prisoner of War Memorial
        {.latLng={{47.48535,-122.174513},{47.486199,-122.173564},{47.486325,-122.172827}}},
        //Metamorphosis - Memorial Fountain - South Lawn Memorial
        {.latLng={{47.48793,-122.173166},{47.486942,-122.171348},{47.485165,-122.172433}}},
        //Garden of Harmony - Iu-Mienh Community Memorial  - Fountain of Youth
        {.latLng={{47.486712,-122.173687},{47.48535,-122.174513},{47.486378,-122.174457}}},
        //Iu-Mienh Community Memorial  - South Lawn Memorial - Prisoner of War Memorial
        {.latLng={{47.48535,-122.174513},{47.485165,-122.172433},{47.486325,-122.172827}}},
        //Garden of Harmony - Love Forever Headstone - Remembered Forever
        {.latLng={{47.486712,-122.173687},{47.48752,-122.172903},{47.487686,-122.173709}}},
        //Lullabyland Memorial - South Lawn Memorial - Prisoner of War Memorial
        {.latLng={{47.486297,-122.172124},{47.485165,-122.172433},{47.486325,-122.172827}}},
        //Lullabyland Memorial - Love Forever Headstone - Prisoner of War Memorial
        {.latLng={{47.486297,-122.172124},{47.48752,-122.172903},{47.486325,-122.172827}}},
        //Lullabyland Memorial - Sun Dial - South Lawn Memorial
        {.latLng={{47.486297,-122.172124},{47.486194,-122.171397},{47.485165,-122.172433}}},
        //Garden of Harmony - Iu-Mienh Community Memorial  - Fish Fountain
        {.latLng={{47.486712,-122.173687},{47.48535,-122.174513},{47.486199,-122.173564}}}
    };    

    cellId = S2CellId::FromToken("549067e1", 8);
    S2Cell swCell(cellId);
    S2Polygon swPolygon(swCell);
    cellId = S2CellId::FromToken("549067df", 8);
    S2Cell neCell(cellId);
    S2Polygon nePolygon(neCell);

    std::strcpy(delimeter, "");
    for (auto & f : fields) {
        std::vector<S2Point> loopPoints;
        loopPoints.push_back(S2Point(S2LatLng::FromDegrees(f.latLng[0][0],f.latLng[0][1]).Normalized()));
        loopPoints.push_back(S2Point(S2LatLng::FromDegrees(f.latLng[1][0],f.latLng[1][1]).Normalized()));
        loopPoints.push_back(S2Point(S2LatLng::FromDegrees(f.latLng[2][0],f.latLng[2][1]).Normalized()));
        std::unique_ptr<S2Loop> loop(new S2Loop());
        loop->Init(loopPoints);
        S2Polygon fieldPoly(std::move(loop));
        double area = fieldPoly.GetArea();

        std::vector<S2Point> reorderedLoopPoints;
        reorderedLoopPoints.push_back(S2Point(S2LatLng::FromDegrees(f.latLng[2][0],f.latLng[2][1]).Normalized()));
        reorderedLoopPoints.push_back(S2Point(S2LatLng::FromDegrees(f.latLng[1][0],f.latLng[1][1]).Normalized()));
        reorderedLoopPoints.push_back(S2Point(S2LatLng::FromDegrees(f.latLng[0][0],f.latLng[0][1]).Normalized()));
        std::unique_ptr<S2Loop> reorderedLoop(new S2Loop());
        reorderedLoop->Init(reorderedLoopPoints);
        S2Polygon reorderedFieldPoly(std::move(reorderedLoop));
        if (reorderedFieldPoly.GetArea() < area) {
            std::unique_ptr<S2Loop> shortLoop(new S2Loop());
            shortLoop->Init(reorderedLoopPoints);            
            fieldPoly.Init(std::move(shortLoop));
        }

        S2Polygon swIntersection;
        swIntersection.InitToIntersection(&swPolygon, &fieldPoly);

        S2Polygon neIntersection;
        neIntersection.InitToIntersection(&nePolygon, &fieldPoly);

        // Area is going to be computed on the javascript side
        //std::printf("Intersection with 549067e1 in the south west:\n");
        std::printf("%s", delimeter);
        std::strcpy(delimeter, ",\n");
        std::printf("            {\n");
        std::printf("                intersect549067e1:[\n");

        /*
            {        
                intersect549067e1:[
                    new google.maps.LatLng({lat:47.486942,lng:-122.171348}),
                    new google.maps.LatLng({lat:47.487227,lng:-122.170926}),
                    new google.maps.LatLng({lat:47.487984,lng:-122.171882}),
                    new google.maps.LatLng({lat:47.487930,lng:-122.173166}),
                    new google.maps.LatLng({lat:47.486942,lng:-122.171348})
                ], 
                intersect549067df:[
                    new google.maps.LatLng({lat:47.487984,lng:-122.171882}),
                    new google.maps.LatLng({lat:47.487227,lng:-122.170926}),
                    new google.maps.LatLng({lat:47.488078,lng:-122.169664}),
                    new google.maps.LatLng({lat:47.487984,lng:-122.171882})
                ]
            },
        */

       char pointDelimiter[8];
       std::strcpy(pointDelimiter, "");
        for (int l = 0; l < swIntersection.num_loops(); l++) {
            for (int v = 0; v <= swIntersection.loop(l)->num_vertices(); v++) {
                std::printf("%s", pointDelimiter);
                strcpy(pointDelimiter, ",\n");
                std::printf("                    new google.maps.LatLng(");
                S2LatLng point(swIntersection.loop(l)->vertex(v));
                std::printf(
                    "{lat:%f,lng:%f})",
                    point.lat().degrees(),
                    point.lng().degrees()
                );
            }
        }
        std::printf("\n");

        //std::printf("\nIntersection with 549067df in the north east:\n");
        //std::printf("            {\n");
        std::printf("                ],\n");
        std::printf("                intersect549067df:[\n");

        std::strcpy(pointDelimiter, "");

        for (int l = 0; l < neIntersection.num_loops(); l++) {
            for (int v = 0; v <= neIntersection.loop(l)->num_vertices(); v++) {
                std::printf("%s", pointDelimiter);
                strcpy(pointDelimiter, ",\n");
                std::printf("                    new google.maps.LatLng(");
                S2LatLng point(neIntersection.loop(l)->vertex(v));
                std::printf(
                    "{lat:%f,lng:%f})",
                    point.lat().degrees(),
                    point.lng().degrees()
                );
            }
        }
        std::printf("\n                ]\n");
        std::printf("            }");
    }
    std::printf("\n");    
}
#endif

// Server side C program to demonstrate HTTP Server programming
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <string>
#include <regex>
#include <netdb.h>
#include <map>
#include <sstream>
#include <string>

using namespace std;

void Respond(int socket, string const & result, string const & contentType, string const & content) {
    string message;
    message = "HTTP/1.1 "
        + result
        + "\nContent-Type: " 
        + contentType
        + "\nAccess-Control-Allow-Origin *\nX-Content-Type-Options: nosniff\nContent-Length: "
        + to_string(content.length())
        + "\n\n"
        + content;
    printf("write to socket: %lu byte string\n", message.length());
    write(socket, message.c_str() , message.length());
}

void Respond(int socket, string const & result, string const & contentType, void * content, size_t contentSize) {
    string message;
    message = "HTTP/1.1 "
        + result
        + "\nContent-Type: " 
        + contentType
        + "\nAccess-Control-Allow-Origin *\nX-Content-Type-Options: nosniff\nContent-Length: "
        + to_string(contentSize)
        + "\n\n";
    printf("write to socket: %s plus binary payload of %lu bytes\n", message.c_str(), contentSize);
    write(socket, message.c_str(), message.length());
    write(socket, content, contentSize);
}

void ReportError(int socket, string const & error) {
    Respond(socket, "400 Bad Request", "text/plain", error);
}

void usage(int socket, char const * method, char const * url) {
    string payload("Unrecognized request: ");
    payload += string(method) + " " +string(url);
    ReportError(socket, payload);
}

void ParseArgs(string const & args, map<string, string> * results) {
    vector<string> tokens;
    string token;
    istringstream tokenStream(args);
    while (getline(tokenStream, token, '&'))
    {
        tokens.push_back(token);
    }
    for (string const & t : tokens) {
        char const * tstr = t.c_str();
        char const * equals = strchr(tstr, '=');
        if (equals) {
            string lhs(tstr, equals);
            string rhs(equals+1);
            results->emplace(lhs, rhs);
        }
    }
}

void ParseRect(string const & input, S2LatLngRect *rect) {
    // input of the form lat,lng_lat,lng_lat,lng_lat,lng
    vector<string> tokens;
    string token;
    istringstream tokenStream(input);
    while (getline(tokenStream, token, '_'))
    {
        tokens.push_back(token);
    }
    for (string const & t : tokens) {
        char const * tstr = t.c_str();
        char const * comma = strchr(tstr, ',');
        if (comma) {
            string lhs(tstr, comma);
            string rhs(comma+1);
            S2LatLng point;
            point = S2LatLng::FromDegrees(::atof(lhs.c_str()), ::atof(rhs.c_str())).Normalized();
            rect->AddPoint(point);
        }
    }
}

void ParseCellIds(string const & input, vector<S2Polygon *> *polygons, vector<string> *cellIds) {
    // input of the form cellID_cellID_... 
    string token;
    istringstream tokenStream(input);
    while (getline(tokenStream, token, '_'))
    {
        cellIds->push_back(token);
    }
    for (string const & t : *cellIds) {
        S2CellId cellId = S2CellId::FromToken(t.c_str(), t.length());
        S2Cell cell(cellId);
        S2Polygon * poly = new S2Polygon(cell);
        polygons->push_back(poly);
    }
}

void ParsePolygons(string const & input, vector<S2Polygon *> *polygons) {
    // input of the form lat,lng_lat,lng_lat,lng+lat,lng_lat,lng_lat,lng+...
    printf("ParsePolygons: %s\n", input.c_str());
    string polygonToken;
    istringstream tokenStream(input);
    while (getline(tokenStream, polygonToken, '+'))
    {
        // This token represents a single polygon of the form lat,lng_lat,lng_lat,lng_...
        istringstream polygonStream(polygonToken);
        printf("Polygon token: %s\n", polygonToken.c_str());
        
        string token;
        vector<string> tokens;
        while (getline(polygonStream, token, '_'))
        {
            // Each of these tokens is a lat,lng pair
            tokens.push_back(token);
            printf("lat,lng: %s\n", token.c_str());
        }
        std::vector<S2Point> loopPoints;
        for (string const & t : tokens) {
            char const * tstr = t.c_str();
            char const * comma = strchr(tstr, ',');
            if (comma) {
                string lhs(tstr, comma);
                string rhs(comma+1);
                S2LatLng point;
                point = S2LatLng::FromDegrees(::atof(lhs.c_str()), ::atof(rhs.c_str())).Normalized();
                loopPoints.push_back(S2Point(point));
            }
        }
        std::unique_ptr<S2Loop> loop(new S2Loop());
        loop->Init(loopPoints);
        S2Polygon * fieldPoly = new S2Polygon(std::move(loop));
        double area = fieldPoly->GetArea();

        std::vector<S2Point> reorderedLoopPoints(loopPoints);
        std::reverse(reorderedLoopPoints.begin(),reorderedLoopPoints.end()); 
        std::unique_ptr<S2Loop> reorderedLoop(new S2Loop());
        reorderedLoop->Init(reorderedLoopPoints);
        S2Polygon reorderedFieldPoly(std::move(reorderedLoop));
        if (reorderedFieldPoly.GetArea() < area) {
            std::unique_ptr<S2Loop> shortLoop(new S2Loop());
            shortLoop->Init(reorderedLoopPoints);            
            fieldPoly->Init(std::move(shortLoop));
        }

        polygons->push_back(fieldPoly);
    }
}

void CellBounds(int socket, vector<S2CellId> const & cells) {
    constexpr int maxCells = 10000;
    int cellCount=0;
    string ids("");
    string bounds("");
    string cellDelimiter("");
    for (auto id : cells) {
        if (cellCount++ > maxCells) {
            Respond(socket, "403 Forbidden", "text/plain", "Too many cells requested");
            return;
        }
        ids += cellDelimiter + string("\"") + id.ToToken() + "\"";

        S2Cell cell(id);
        string delimeter("");
        bounds += cellDelimiter + "[";
        for (int i=0; i<4; i++) {
            S2LatLng point(cell.GetVertex(i));
            bounds += delimeter 
                    + "{\"lat\":"
                    + to_string(point.lat().degrees())
                    + ", \"lng\":"
                    + to_string(point.lng().degrees())
                    + "}"; 
            delimeter = ",";
        }
        bounds += "]";

        cellDelimiter = ",";
    }
    string result = "{\"ids\":[" + ids + "],\"bounds\":[" + bounds + "]}";

    Respond(socket, "200 OK", "text/plain", result);
}

void CoverFields(int socket, int minLevel, int maxLevel, vector<S2Polygon *> const & fields) {
    S2RegionCoverer::Options options;
    options.set_max_cells(100000);
    options.set_min_level(minLevel);
    options.set_max_level(maxLevel);
    S2RegionCoverer coverer(options);

    vector<S2CellId> cells;
    for (auto field : fields) {
        vector<S2CellId> newCells;
        coverer.GetCovering(*field, &newCells);
        for (auto cell : newCells) {
            if (std::find(cells.begin(), cells.end(), cell) == cells.end()) {
                cells.push_back(cell);
            }
        }
    }
    CellBounds(socket, cells);
}

void GenerateCover(int socket, map<string,string> const & params) {
    // params are:
    // minLevel int 
    // maxLevel int 
    // rectPoints lat,lng_lat,lng_lat,lng_lat,lng
    map<string,string>::const_iterator it;
    it = params.find(string("minlevel"));
    if (it == params.end()) {
        ReportError(socket, "COVER request requires MINLEVEL argument");
        return;
    }
    int minLevel = atoi(it->second.c_str());
    if (minLevel < 0 || minLevel > S2::kMaxCellLevel) {
        ReportError(
            socket, 
            string("COVER request requires MINLEVEL between 0 and ")
            + to_string(S2::kMaxCellLevel)
        );
        return;
    }
    it = params.find(string("maxlevel"));
    if (it == params.end()) {
        ReportError(socket, "COVER request requires MAXLEVEL argument");
        return;
    }
    int maxLevel = atoi(it->second.c_str());
    if (maxLevel < 0 || maxLevel > S2::kMaxCellLevel) {
        ReportError(
            socket, 
            string("COVER request requires MAXLEVEL between 0 and ")
            + to_string(S2::kMaxCellLevel)
        );
        return;
    }
    it = params.find(string("rectpoints"));
    if (it == params.end()) {
        // Also allow fields
        it = params.find(string("fields"));
        if (it == params.end()) {
            ReportError(socket, "COVER request requires RECTPOINTS or FIELDS argument");
            return;
        }
        vector<S2Polygon *> fields;
        printf("Parsing polygons\n");
        ParsePolygons(it->second, &fields);
        if (fields.empty()) {
            ReportError(socket, "COVER request FIELDS argument invalid");
            return;        
        }
        CoverFields(socket, minLevel, maxLevel, fields);
        return;
    }
    S2LatLngRect rect;
    ParseRect(it->second, &rect);
    if (!rect.is_valid() || rect.is_empty() || rect.is_full() || rect.is_point()) {
        ReportError(socket, "COVER request RECTPOINTS argument invalid");
        return;        
    }

    S2RegionCoverer::Options options;
    options.set_max_cells(100000);
    options.set_min_level(minLevel);
    options.set_max_level(maxLevel);
    S2RegionCoverer coverer(options);

    vector<S2CellId> cells;
    coverer.GetCovering(rect, &cells);

    //string result("{\"cells\":[");
    constexpr int maxCells = 10000;
    int cellCount=0;
    string result("[");
    string cellDelimiter("");
    for (auto id : cells) {
        if (cellCount++ > maxCells) {
            Respond(socket, "403 Forbidden", "text/plain", "Too many cells requested");
            return;
        }
        result += cellDelimiter;
        cellDelimiter = ",";
        S2Cell cell(id);
        string delimeter("");
        result += string("{\"name\": \"") + id.ToToken() + "\",\"points\":[";
        for (int i=0; i<4; i++) {
            S2LatLng point(cell.GetVertex(i));
            result += delimeter 
                    + "{\"lat\":"
                    + to_string(point.lat().degrees())
                    + ", \"lng\":"
                    + to_string(point.lng().degrees())
                    + "}"; 
            delimeter = ",";
        }
        result += "]}";
    }
    //result += "]}";
    result += "]";

    //Respond(socket, "200 OK", "text/javascript", result);
    Respond(socket, "200 OK", "text/plain", result);
    //Respond(socket, "200 OK", "application/json; charset=UTF-8", result);

    // Area right around Jimi in case we need much smaller cells
    //point = S2LatLng::FromDegrees(47.490477, -122.178857).Normalized();
    //rect.AddPoint(point);
    //point = S2LatLng::FromDegrees(47.490900, -122.164250).Normalized();
    //rect.AddPoint(point);
    //point = S2LatLng::FromDegrees(47.483857, -122.164184).Normalized();
    //rect.AddPoint(point);
    //point = S2LatLng::FromDegrees(47.484339, -122.179119).Normalized();
    //rect.AddPoint(point);
    /*
    47.490477,-122.178857_47.490900,-122.164250_47.483857,-122.164184_47.484339,-122.179119
    */
}

void Children(int socket, map<string,string> const & params) {
    // params are:
    // cell string 
    map<string,string>::const_iterator it;
    it = params.find(string("cell"));
    if (it == params.end()) {
        ReportError(socket, "CHILDREN request requires CELL argument");
        return;
    }
    S2CellId cellId = S2CellId::FromToken(it->second.c_str(), it->second.length());
    vector<S2CellId> cells;
    for (int i=0; i<4; i++) {
        cells.push_back(cellId.child(i));
    }
    CellBounds(socket, cells);
}

void FindIntersections(int socket, map<string,string> const & params) {
    // params are:
    // cells cellID_cellID_... 
    // fields lat,lng_lat,lng_lat,lng-lat,lng_lat,lng_lat,lng-...
    printf("Finding intersections\n");
    map<string,string>::const_iterator it;
    it = params.find(string("cells"));
    if (it == params.end()) {
        ReportError(socket, "INTERSECT request requires CELLS argument");
        return;
    }
    vector<S2Polygon *> cells;
    vector<string> cellIds;
    ParseCellIds(it->second, &cells, &cellIds);
    if (cells.empty()) {
        ReportError(socket, "INTERSECT request CELLS argument invalid");
        return;        
    }
    it = params.find(string("fields"));
    if (it == params.end()) {
        ReportError(socket, "INTERSECT request requires FIELDS argument");
        return;
    }
    vector<S2Polygon *> fields;
    printf("Parsing polygons\n");
    ParsePolygons(it->second, &fields);
    if (fields.empty()) {
        ReportError(socket, "INTERSECT request FIELDS argument invalid");
        return;        
    }
    printf("Polygons parsed\n");

    string result("{");
    string cellDelimiter("");
    for (int i=0; i<cells.size();i++) {
        result += cellDelimiter + "\"" + cellIds[i] + "\":[";
        cellDelimiter =  ",";
        string polyDelimiter("");
        for (auto & fieldPoly : fields) {
            S2Polygon intersection;
            intersection.InitToIntersection(cells[i], fieldPoly);
            result += polyDelimiter + "[";
            polyDelimiter = ",";
            string pointDelimiter("");
            for (int l = 0; l < intersection.num_loops(); l++) {
                for (int v = 0; v <= intersection.loop(l)->num_vertices(); v++) {
                    S2LatLng point(intersection.loop(l)->vertex(v));
                    result += pointDelimiter 
                            + "{\"lat\":"
                            + to_string(point.lat().degrees())
                            + ", \"lng\":"
                            + to_string(point.lng().degrees())
                            + "}"; 
                    pointDelimiter = ",";
                }
            }
            result += "]";            
        }
        result += "]";
    }
    result += "}";
    for (auto cell : cells) {
        delete cell;
    }
    for (auto field : fields) {
        delete field;
    }    
    Respond(socket, "200 OK", "text/plain", result);
}

// CORS and CORB seem to make it impossible to run http-server to serve local web pages on 8080
// and have those web pages make requests to get data from this service running on 5000.
// localhost:8080 and localhost:5000 are different domains.  Pfffttt.
// So serve the local pages from here.  I guess.
void ServeFile(int socket, char const * method, char const * uri) {
    // Assume no query string, uri is just a file name and extension
    // Assume no subdirectories, put your scripts and css in the same folder as the html
    // Assume just html, js, json, css, png.  Start with html.  It's requested first. :-)
    char * url = (char *)malloc(strlen(uri));
    strcpy(url, uri);
    char * dot = strchr(url, '.');
    if (!dot) {
        usage(socket, method, url);
        return;
    }
    char * qmark = strchr(url, '?');
    if (qmark) *qmark = '\0';  // Be argumentative on some other server!

    if (*url == '/') url++;

    bool binary = false;
    string contentType("text/html");
    if (!strcmp(dot, ".html")) {
        contentType = "text/html";
    }
    else if (!strcmp(dot, ".json")) {
        contentType = "application/json; charset=UTF-8";
    }
    else if (!strcmp(dot, ".png")) {
        binary = true;
        contentType = "image/png";
    }
    else if (!strcmp(dot, ".ico")) {
        binary = true;
        contentType = "image/vnd.microsoft.icon";
    }
    else {
        ReportError(socket, string("unsupported file type ") + (dot+1));
        return;
    }
    FILE * sourceFile = fopen(url, binary? "rb" : "r");
    if (!sourceFile) {
        ReportError(socket, string("Could't open file ") + url);
        char cwd[1024];
        getcwd(cwd, sizeof(cwd));
        printf("Current working dir: %s\n", cwd);            
        return;
    }
    fseek (sourceFile, 0, SEEK_END);
    auto length = ftell (sourceFile);
    fseek (sourceFile, 0, SEEK_SET);
    void * buffer = malloc(length+1);
    if (buffer)
    {
        fread (buffer, 1, length, sourceFile);
    }
    fclose (sourceFile);
    if (binary) {
        Respond(socket, "200 OK", contentType, buffer, length);
    }        
    else {
        ((char *)buffer)[length] = '\0';
        Respond(socket, "200 OK", contentType, (char *)buffer);
    }
}

#define PORT 5000
int main(int argc, char const *argv[])
{
    Aws::SDKOptions options;
    Aws::InitAPI(options);
    try {

    int server_fd, new_socket; long valread;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
        
    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("In socket");
        exit(EXIT_FAILURE);
    }
    
    int port = PORT;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( PORT );
    
    memset(address.sin_zero, '\0', sizeof address.sin_zero);
    
    
    while (::bind(server_fd, (struct sockaddr *)&address, sizeof(address))<0)
    {
        port++;
        address.sin_port = htons(port);
    }
    if (listen(server_fd, 10) < 0)
    {
        perror("In listen");
        exit(EXIT_FAILURE);
    }

    char hoststr[NI_MAXHOST];
    char portstr[NI_MAXSERV];

    int rc = getnameinfo((struct sockaddr *)&address, 
        sizeof(struct sockaddr_storage), hoststr, sizeof(hoststr), portstr, sizeof(portstr), 
        NI_NUMERICHOST | NI_NUMERICSERV);

    while(1)
    {
        printf("\n Listening on %s:%s\n\n", hoststr, portstr);
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0)
        {
            perror("In accept");
            exit(EXIT_FAILURE);
        }
        /* Create child process */
        int pid = fork();
            
        if (pid < 0) {
            perror("ERROR on fork");
            exit(1);
        }        
        if (pid != 0) {
            close(new_socket);
            continue; // not child process
        }
        close(server_fd);
        
        constexpr int buffer_size = 30000;
        char buffer[buffer_size] = {0};
        valread = read( new_socket , buffer, buffer_size);
        printf("%s\n",buffer );
        printf("(%ld bytes)\n", valread);
        /*        
        constexpr int buffer_size = 1024 * 1024;
        char buffer[buffer_size] = {0};
        valread = -1;
        long bytesRead = 0;
        while (valread != 0) {
            valread = read( new_socket , buffer+bytesRead, buffer_size-bytesRead);
            if (valread > 0)
                bytesRead += valread;
            if (bytesRead >= buffer_size) {
                printf("BUFFER OVERFLOW!!!");
            }
        }
        printf("%s\n",buffer );
        */

        string s(buffer);
        regex ws_re("\\s+");
        regex line_re("[\\r\\n]+");
        vector<string> lines{
            sregex_token_iterator(s.begin(), s.end(), line_re, -1), {}
        };
        int i=0;
        int contentLength=0;
        for (auto & token : lines) {
            printf("line %d: %s\n", i++, token.c_str());
            printf("   (%ld bytes)\n", strlen(token.c_str()));
            if (!strncmp(token.c_str(), "Content-Length:", strlen("Content-Length:"))) {
                contentLength = atoi(token.c_str() + strlen("Content-Length:"));
                printf("  Content-Length = %d\n", contentLength);
            }
        }
        if (contentLength && strlen(lines.back().c_str()) < contentLength) {
            printf("Didn't read all of the message body\n");
            int bytesRead;
            // This is some hoaky hacky crap
            if (!strncmp(lines.back().c_str(), "Accept-Language:", strlen("Accept-Language:"))) {
                bytesRead = 0;
                printf("Didn't read any of the message body.\n");
                lines.push_back("");
            }
            else {
                bytesRead = strlen(lines.back().c_str());
            }
            while (bytesRead < contentLength) {
                memset(buffer, '\0', buffer_size);
                valread = read( new_socket , buffer, buffer_size);
                if (valread < 0) {
                    printf("ERROR! How do we find the message body?  It's buried here somewhere.");
                }
                else if (valread > 0) {
                    bytesRead += valread;
                    lines.back() += buffer;
                }
            }
        }
        else {
            printf("Read all of the message body\n");
        }
        vector<string> tokens{
            sregex_token_iterator(lines[0].begin(), lines[0].end(), ws_re, -1), {}
        };
        //printf("Tokens of line 0: [\n");
        //for (auto & token : tokens) {
        //    printf("%s\n", token.c_str());
        //}
        //printf("]\n");
        char const * uri = tokens[1].c_str();
        printf("endpoint requested: %s\n", uri);
        char const * qmark = strchr(uri, '?');
        if (qmark) {
            string action(uri+1, qmark);
            transform(action.begin(), action.end(), action.begin(), ::tolower);
            string params(qmark+1);
            transform(params.begin(), params.end(), params.begin(), ::tolower);
            printf("action: %s\nparams: %s\n", action.c_str(), params.c_str());
            if (action == "cover") {
                map<string,string> arguments;
                params += "&" + lines.back();
                ParseArgs(params, &arguments);
                GenerateCover(new_socket, arguments);
            }
            else if (action == "intersect") {
                map<string,string> arguments;
                params += "&" + lines.back();
                ParseArgs(params, &arguments);
                FindIntersections(new_socket, arguments);
            }
            else if (action == "children") {
                map<string,string> arguments;
                ParseArgs(params, &arguments);
                Children(new_socket, arguments);
            }
            else {
                //usage(new_socket, tokens[0].c_str(), uri);
                ServeFile(new_socket, tokens[0].c_str(), uri);
            }
        }
        else {
            //usage(new_socket, tokens[0].c_str(), uri);
            ServeFile(new_socket, tokens[0].c_str(), uri);
        }

        printf("------------------message sent-------------------");
        //close(new_socket);
        exit(0);
    }
    } catch (...) {
        Aws::ShutdownAPI(options);    
    }
    return 0;
}