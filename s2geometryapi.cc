// s2geometryapi.cc
#include <cstdio>
#include <map>
#include <memory.h>
#include <regex>
#include <sstream>
#include <stdlib.h>
#include <string>
#include <string.h>
#include "s2/s2region_coverer.h"
#include "s2/s2region.h"
#include "s2/s2latlng.h"
#include "s2/s2latlng_rect.h"
#include "s2/s2polygon.h"

using namespace std;

void Respond(int result, string const & content) {
    printf("%s", content.c_str());
    exit(result);
}

void ReportError(string const & error) {
    Respond(400, error);
}

void Usage(char const * method, char const * url) {
    string payload("Unrecognized request: ");
    payload += string(method) + " " + string(url);
    ReportError(payload);
}

string UrlDecode(string const & source) {
    string result;
    int i, hex;
    for (i = 0; i < source.length(); ) {
        if (int(source[i]) == '%') {
            sscanf(source.substr(i + 1, 2).c_str(), "%x", &hex);
            result += static_cast<char>(hex);
            i += 3;
        } else {
            result += source[i++];
        }
    }
    return result;
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
            std::transform(lhs.begin(), lhs.end(), lhs.begin(), [](unsigned char c){ return std::tolower(c); });            
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
    //printf("ParsePolygons: %s\n", input.c_str());
    string polygonToken;
    istringstream tokenStream(input);
    while (getline(tokenStream, polygonToken, '+'))
    {
        // This token represents a single polygon of the form lat,lng_lat,lng_lat,lng_...
        istringstream polygonStream(polygonToken);
        //printf("Polygon token: %s\n", polygonToken.c_str());
        
        string token;
        vector<string> tokens;
        while (getline(polygonStream, token, '_'))
        {
            // Each of these tokens is a lat,lng pair
            tokens.push_back(token);
            //printf("lat,lng: %s\n", token.c_str());
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

void CellBounds(vector<S2CellId> const & cells) {
    constexpr int maxCells = 10000;
    int cellCount=0;
    string ids("");
    string bounds("");
    string cellDelimiter("");
    for (auto id : cells) {
        if (cellCount++ > maxCells) {
            Respond(403, "Too many cells requested");
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

    Respond(200, result);
}

void CoverFields(int minLevel, int maxLevel, vector<S2Polygon *> const & fields) {
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
    CellBounds(cells);
}

void GenerateCover(map<string,string> const & params) {
    // params are:
    // minLevel int 
    // maxLevel int 
    // rectPoints lat,lng_lat,lng_lat,lng_lat,lng
    map<string,string>::const_iterator it;
    it = params.find(string("minlevel"));
    if (it == params.end()) {
        ReportError("COVER request requires MINLEVEL argument");
        return;
    }
    int minLevel = atoi(it->second.c_str());
    if (minLevel < 0 || minLevel > S2::kMaxCellLevel) {
        ReportError(
            string("COVER request requires MINLEVEL between 0 and ")
            + to_string(S2::kMaxCellLevel)
        );
        return;
    }
    it = params.find(string("maxlevel"));
    if (it == params.end()) {
        ReportError("COVER request requires MAXLEVEL argument");
        return;
    }
    int maxLevel = atoi(it->second.c_str());
    if (maxLevel < 0 || maxLevel > S2::kMaxCellLevel) {
        ReportError(
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
            ReportError("COVER request requires RECTPOINTS or FIELDS argument");
            return;
        }
        vector<S2Polygon *> fields;
        //printf("Parsing polygons\n");
        ParsePolygons(it->second, &fields);
        if (fields.empty()) {
            ReportError("COVER request FIELDS argument invalid");
            return;        
        }
        CoverFields(minLevel, maxLevel, fields);
        return;
    }
    S2LatLngRect rect;
    ParseRect(it->second, &rect);
    if (!rect.is_valid() || rect.is_empty() || rect.is_full() || rect.is_point()) {
        ReportError("COVER request RECTPOINTS argument invalid");
        return;        
    }

    S2RegionCoverer::Options options;
    options.set_max_cells(100000);
    options.set_min_level(minLevel);
    options.set_max_level(maxLevel);
    S2RegionCoverer coverer(options);

    vector<S2CellId> cells;
    coverer.GetCovering(rect, &cells);

    constexpr int maxCells = 10000;
    int cellCount=0;
    string result("[");
    string cellDelimiter("");
    for (auto id : cells) {
        if (cellCount++ > maxCells) {
            Respond(403, "Too many cells requested");
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
    result += "]";

    Respond(200, result);
}

void Children(map<string,string> const & params) {
    // params are:
    // cell string 
    map<string,string>::const_iterator it;
    it = params.find(string("cell"));
    if (it == params.end()) {
        ReportError("CHILDREN request requires CELL argument");
        return;
    }
    S2CellId cellId = S2CellId::FromToken(it->second.c_str(), it->second.length());
    vector<S2CellId> cells;
    for (int i=0; i<4; i++) {
        cells.push_back(cellId.child(i));
    }
    CellBounds(cells);
}

void FindIntersections(map<string,string> const & params) {
    // params are:
    // cells cellID_cellID_... 
    // fields lat,lng_lat,lng_lat,lng-lat,lng_lat,lng_lat,lng-...
    //printf("Finding intersections\n");
    map<string,string>::const_iterator it;
    it = params.find(string("cells"));
    if (it == params.end()) {
        ReportError("INTERSECT request requires CELLS argument");
        return;
    }
    vector<S2Polygon *> cells;
    vector<string> cellIds;
    ParseCellIds(it->second, &cells, &cellIds);
    if (cells.empty()) {
        ReportError("INTERSECT request CELLS argument invalid");
        return;        
    }
    it = params.find(string("fields"));
    if (it == params.end()) {
        ReportError("INTERSECT request requires FIELDS argument");
        return;
    }
    vector<S2Polygon *> fields;
    //printf("Parsing polygons\n");
    ParsePolygons(it->second, &fields);
    if (fields.empty()) {
        ReportError("INTERSECT request FIELDS argument invalid");
        return;        
    }
    //printf("Polygons parsed\n");

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
    Respond(200, result);
}

int main(int argc, char const *argv[])
{
    if (argc == 2)
        Usage(argv[1], "<null>");
    else if (argc == 1)
        Usage("<null>", "<null>");

    string action(argv[1]);
    transform(action.begin(), action.end(), action.begin(), ::tolower);

    map<string,string> arguments;
    ParseArgs(UrlDecode(argv[2]), &arguments);    
    if (action == "cover") {
        GenerateCover(arguments);
    }
    else if (action == "intersect") {
        FindIntersections(arguments);
    }
    else if (action == "children") {
        Children(arguments);
    }
    else {
        Usage(argv[1], argv[2]);
    }

    return 0;
}