#pragma once
// Reads a map exported from Tiled as JSON. Plain C++ (no Qt) so it can be unit-tested.
//
// Conventions (agree on these once, then never change them):
//   Tile layer "Collision"  : any non-empty tile is solid. Hidden when drawn.
//   Other tile layers       : drawn in order (Ground, Walls, Decor, ...).
//   Object layer            : objects with a Class of  clue | door | npc | spawn
//       clue  -> custom property  clue_id    (string)
//       door  -> custom property  to         (string, location id)
//       npc   -> custom property  suspect_id (string)
//       spawn -> a point object named "start"
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include "JsonLite.h"
#include "TileMap.h"

struct MapObject {
    int id = 0;
    std::string name, type;
    float x = 0, y = 0, w = 0, h = 0;
    std::map<std::string, std::string> props;
    std::string prop(const std::string& k) const {
        auto it = props.find(k);
        return it == props.end() ? "" : it->second;
    }
    float cx() const { return x + w / 2; }
    float cy() const { return y + h / 2; }
};

struct TileLayer {
    std::string name;
    bool visible = true;
    std::vector<unsigned> data;  // one gid per tile, row by row; 0 = empty
};

struct TilesetInfo {
    int firstGid = 1, columns = 0, tileW = 32, tileH = 32, margin = 0, spacing = 0, tileCount = 0;
    std::string image;
};

class TiledMap {
public:
    static TiledMap fromFile(const std::string& path) {
        std::ifstream f(path, std::ios::binary);
        if (!f) throw std::runtime_error("Cannot open map file: " + path);
        std::stringstream ss;
        ss << f.rdbuf();
        size_t slash = path.find_last_of("/\\");
        return fromString(ss.str(), slash == std::string::npos ? "" : path.substr(0, slash));
    }

    static TiledMap fromString(const std::string& text, const std::string& baseDir = "") {
        Json root = Json::parse(text);
        TiledMap m;
        m.baseDir_ = baseDir;
        m.width_ = static_cast<int>(root["width"].number());
        m.height_ = static_cast<int>(root["height"].number());
        m.tileW_ = static_cast<int>(root["tilewidth"].number(32));
        m.tileH_ = static_cast<int>(root["tileheight"].number(32));
        if (m.width_ <= 0 || m.height_ <= 0) throw std::runtime_error("Map has no width/height");

        const Json& layers = root["layers"];
        for (size_t li = 0; li < layers.size(); ++li) {
            const Json& L = layers[li];
            std::string type = L["type"].string();
            if (type == "tilelayer") {
                TileLayer tl;
                tl.name = L["name"].string();
                tl.visible = L["visible"].boolean(true);
                if (!L["data"].isArray())
                    throw std::runtime_error("Layer '" + tl.name + "': set Map > Tile Layer Format to CSV in Tiled");
                for (size_t k = 0; k < L["data"].size(); ++k)
                    tl.data.push_back(static_cast<unsigned>(L["data"][k].number()) & 0x1FFFFFFFu);  // strip flip flags
                if (tl.data.size() != static_cast<size_t>(m.width_ * m.height_))
                    throw std::runtime_error("Layer '" + tl.name + "' has the wrong number of tiles");
                m.layers_.push_back(std::move(tl));
            } else if (type == "objectgroup") {
                const Json& objs = L["objects"];
                for (size_t k = 0; k < objs.size(); ++k) m.objects_.push_back(readObject(objs[k]));
            }
        }

        const Json& ts = root["tilesets"];
        if (ts.size() > 0) {
            const Json& t = ts[static_cast<size_t>(0)];
            if (t.has("source"))
                throw std::runtime_error("Tileset is external: in Tiled click 'Embed Tileset' in the Tilesets panel");
            m.tileset_.firstGid = static_cast<int>(t["firstgid"].number(1));
            m.tileset_.columns = static_cast<int>(t["columns"].number());
            m.tileset_.tileW = static_cast<int>(t["tilewidth"].number(32));
            m.tileset_.tileH = static_cast<int>(t["tileheight"].number(32));
            m.tileset_.margin = static_cast<int>(t["margin"].number());
            m.tileset_.spacing = static_cast<int>(t["spacing"].number());
            m.tileset_.tileCount = static_cast<int>(t["tilecount"].number());
            m.tileset_.image = t["image"].string();
        }
        if (!m.collisionLayer()) throw std::runtime_error("Map needs a tile layer named 'Collision'");
        return m;
    }

    int width() const { return width_; }
    int height() const { return height_; }
    int tileW() const { return tileW_; }
    int tileH() const { return tileH_; }
    const std::vector<TileLayer>& tileLayers() const { return layers_; }
    const std::vector<MapObject>& objects() const { return objects_; }
    const TilesetInfo& tileset() const { return tileset_; }
    std::string tilesetImagePath() const { return baseDir_.empty() ? tileset_.image : baseDir_ + "/" + tileset_.image; }

    const MapObject* findByType(const std::string& type, const std::string& name = "") const {
        for (const auto& o : objects_)
            if (o.type == type && (name.empty() || o.name == name)) return &o;
        return nullptr;
    }

    // Build the solid/walkable grid the Player collides against.
    TileMap collisionMap() const {
        const TileLayer* c = collisionLayer();
        std::vector<std::string> rows;
        for (int ty = 0; ty < height_; ++ty) {
            std::string row;
            for (int tx = 0; tx < width_; ++tx) row += c->data[ty * width_ + tx] ? '#' : '.';
            rows.push_back(row);
        }
        return TileMap(rows, tileW_);
    }

private:
    const TileLayer* collisionLayer() const {
        for (const auto& l : layers_) if (l.name == "Collision") return &l;
        return nullptr;
    }

    static MapObject readObject(const Json& o) {
        MapObject m;
        m.id = static_cast<int>(o["id"].number());
        m.name = o["name"].string();
        m.type = o["class"].string();                    // Tiled 1.9+
        if (m.type.empty()) m.type = o["type"].string(); // older Tiled
        m.x = static_cast<float>(o["x"].number());
        m.y = static_cast<float>(o["y"].number());
        m.w = static_cast<float>(o["width"].number());
        m.h = static_cast<float>(o["height"].number());
        const Json& props = o["properties"];
        for (size_t k = 0; k < props.size(); ++k) {
            const Json& p = props[k];
            const Json& v = p["value"];
            std::string val;
            if (v.type == Json::Type::String) val = v.str;
            else if (v.type == Json::Type::Bool) val = v.b ? "true" : "false";
            else if (v.type == Json::Type::Number) { std::ostringstream os; os << v.num; val = os.str(); }
            m.props[p["name"].string()] = val;
        }
        return m;
    }

    int width_ = 0, height_ = 0, tileW_ = 32, tileH_ = 32;
    std::string baseDir_;
    std::vector<TileLayer> layers_;
    std::vector<MapObject> objects_;
    TilesetInfo tileset_;
};
