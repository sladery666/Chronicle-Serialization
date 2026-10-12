#include "commands.hpp"
#include <iomanip>
#include <nlohmann/json.hpp>
#include <sstream>
namespace chronicle::cli {
namespace {
nlohmann::ordered_json jsonScene(const Scene& scene) {
    nlohmann::ordered_json items = nlohmann::ordered_json::array();
    for (const auto& item : scene.player.inventory) {
        items.push_back({{"id", item.id}, {"name", item.name}});
    }
    return {{"name", scene.name},
            {"player",
             {{"id", scene.player.id},
              {"name", scene.player.name},
              {"stats",
               {{"health", scene.player.stats.health},
                {"maxHealth", scene.player.stats.maxHealth}}},
              {"position",
               {{"x", scene.player.position.x},
                {"y", scene.player.position.y},
                {"z", scene.player.position.z}}},
              {"inventory", items}}}};
}
void inspectObject(std::ostringstream& out, const ObjectView& object,
                   unsigned indent, const SchemaRegistry& registry) {
    const std::string pad(indent, ' ');
    const auto* schema = registry.find(object.type, object.version);
    out << pad << (schema ? schema->name : "Unknown") << " type=" << object.type
        << " schema=" << object.version << " offset=0x" << std::hex
        << object.offset << std::dec << '\n';
    for (const auto& member : object.fields) {
        const FieldSchema* known = nullptr;
        if (schema) {
            for (const auto& f : schema->fields) {
                if (f.id == member.id) {
                    known = &f;
                    break;
                }
            }
        }
        out << pad << "  " << (known ? known->name : "unknown")
            << " id=" << member.id
            << " wire=" << static_cast<unsigned>(member.wire)
            << " size=" << member.payload.size() << " offset=0x" << std::hex
            << member.offset << std::dec << '\n';
        if (!known) {
            continue;
        }
        BinaryReader reader(member.payload, member.offset, "inspect");
        if (member.wire == WireType::object) {
            inspectObject(out, decodeObjectView(reader), indent + 4, registry);
        }
        if (member.wire == WireType::array) {
            const auto count = reader.readU32();
            if (known->elementWire != WireType::object) {
                out << pad << "    elements=" << count << '\n';
                continue;
            }
            for (std::uint32_t i = 0; i < count; ++i) {
                const auto size = reader.readU64();
                const auto offset = reader.offset();
                BinaryReader nested(
                    reader.readBytes(static_cast<std::size_t>(size)), offset,
                    "inspect.item");
                inspectObject(out, decodeObjectView(nested), indent + 4,
                              registry);
            }
        }
    }
}
void diffJson(std::ostringstream& out, const nlohmann::ordered_json& a,
              const nlohmann::ordered_json& b, const std::string& path) {
    if (a.is_object() && b.is_object()) {
        for (auto it = a.begin(); it != a.end(); ++it) {
            diffJson(out, it.value(), b.at(it.key()),
                     path.empty() ? it.key() : path + "." + it.key());
        }
    } else if (a.is_array() && b.is_array()) {
        const auto count = std::min(a.size(), b.size());
        for (std::size_t i = 0; i < count; ++i) {
            diffJson(out, a[i], b[i], path + "[" + std::to_string(i) + "]");
        }
        for (std::size_t i = count; i < a.size(); ++i) {
            out << path << '[' << i << "]: item removed " << a[i].dump()
                << '\n';
        }
        for (std::size_t i = count; i < b.size(); ++i) {
            out << path << '[' << i << "]: item added " << b[i].dump() << '\n';
        }
    } else if (a != b) {
        out << path << ": " << a.dump() << " -> " << b.dump() << '\n';
    }
}
} // namespace
std::string toJson(const Scene& scene) {
    return jsonScene(scene).dump(2) + "\n";
}
std::string inspectRecord(const ObjectRecord& record,
                          const SchemaRegistry& registry) {
    std::ostringstream out;
    ObjectView root{record.type, record.version, {}, record.offset};
    root.fields.reserve(record.fields.size());
    for (const auto& field : record.fields) {
        root.fields.push_back(
            {field.id, field.wire, field.payload.view(), field.offset});
    }
    inspectObject(out, root, 0, registry);
    return out.str();
}
std::string inspect(const LoadedScene& scene) {
    return "Chronicle Lite format=1 root=Scene\n" +
           inspectRecord(scene.root, builtinSchemas());
}
std::string diff(const LoadedScene& before, const LoadedScene& after) {
    std::ostringstream out;
    if (before.playerVersion != after.playerVersion) {
        out << "player.schema: " << before.playerVersion << " -> "
            << after.playerVersion << '\n';
        if (before.playerVersion == 1 && after.playerVersion >= 2) {
            out << "player.name: field added, default = \"Unknown\"\n"
                << "player.health: int32 -> int64\n";
        } else if (before.playerVersion >= 2 && after.playerVersion == 1) {
            out << "player.name: field removed\nplayer.health: int64 -> "
                   "int32\n";
        }
        if (before.playerVersion < 3 && after.playerVersion == 3) {
            out << "player.health: field moved -> player.stats.health\n"
                << "player.stats.maxHealth: field added, value = "
                << after.scene.player.stats.maxHealth << '\n';
        } else if (before.playerVersion == 3 && after.playerVersion < 3) {
            out << "player.stats.health: field moved -> "
                   "player.health\nplayer.stats.maxHealth: field removed\n";
        }
    }
    diffJson(out, jsonScene(before.scene), jsonScene(after.scene), "");
    return out.str();
}
} // namespace chronicle::cli
