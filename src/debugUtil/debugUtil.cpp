/**
 * @file src/debugUtil/debugUtil.cpp
 * @brief Implements debug util facilities for ManuMesh's debug-visualization module.
 * @ingroup manumesh_debug
 *
 * @details Debug visualization is compiled out of release builds and must not affect algorithm results.
 */

#include "debugUtil/debugUtil.h"

#if defined(MANUMESH_ENABLE_DEBUG_UTIL) && !defined(NDEBUG)

#include "algorithms/feature_detection/FeatureTypes.h"
#include "core/Mesh.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace manumesh::debugUtil {
namespace {

/** @brief Concrete HTML color and line width for one debug use case. */
struct PaletteEntry {
    UseCase useCase;
    const char* name;
    const char* color;
    double width;
};

/** @brief Fully resolved edge overlay ready for HTML serialization. */
struct RenderEdge {
    int a = -1;
    int b = -1;
    UseCase useCase = UseCase::Mesh;
    std::string label;
    double width = 1.0;
};

constexpr std::array<PaletteEntry, 10> kPalette = {{
    {UseCase::Mesh, "mesh", "#8a8f98", 1.0},
    {UseCase::Boundary, "boundary", "#2f80ed", 2.5},
    {UseCase::Feature, "feature", "#f2994a", 3.0},
    {UseCase::WeakFeature, "weak-feature", "#9b51e0", 2.5},
    {UseCase::FeatureLoop, "feature-loop", "#27ae60", 3.5},
    {UseCase::Candidate, "candidate", "#56ccf2", 3.0},
    {UseCase::Accepted, "accepted", "#00c853", 3.5},
    {UseCase::Rejected, "rejected", "#eb5757", 3.5},
    {UseCase::Warning, "warning", "#f2c94c", 3.0},
    {UseCase::Error, "error", "#ff1744", 4.0},
}};

const PaletteEntry& palette(UseCase useCase) {
    for (const PaletteEntry& entry : kPalette) {
        if (entry.useCase == useCase) {
            return entry;
        }
    }
    return kPalette[0];
}

bool validEdge(const Mesh& mesh, int a, int b) {
    return a >= 0 && b >= 0 && a < static_cast<int>(mesh.vertices.size()) &&
           b < static_cast<int>(mesh.vertices.size()) && a != b;
}

std::string sanitizeTag(const char* rawTag) {
    const std::string tag = rawTag && rawTag[0] != '\0' ? rawTag : "debug";
    std::string sanitized;
    sanitized.reserve(std::min<std::size_t>(tag.size(), 80));
    // Tags become filenames, so keep them stable across shells and filesystems.
    for (char ch : tag) {
        const auto uch = static_cast<unsigned char>(ch);
        if (std::isalnum(uch) || ch == '-' || ch == '_' || ch == '.') {
            sanitized.push_back(ch);
        } else {
            sanitized.push_back('_');
        }
        if (sanitized.size() >= 80) {
            break;
        }
    }
    return sanitized.empty() ? "debug" : sanitized;
}

std::filesystem::path outputDirectory() {
    if (const char* env = std::getenv("MANUMESH_DEBUG_UTIL_DIR")) {
        if (env[0] != '\0') {
            return std::filesystem::path(env);
        }
    }

    std::error_code ec;
    std::filesystem::path temp = std::filesystem::temp_directory_path(ec);
    if (ec || temp.empty()) {
        temp = std::filesystem::current_path(ec);
    }
    return temp / "manumesh-debugUtil";
}

std::filesystem::path makeOutputPath(const char* tag) {
    static std::atomic<int> counter{0};
    const int id = ++counter;
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

    std::ostringstream name;
    name << millis << "_" << std::setw(3) << std::setfill('0') << id << "_" << sanitizeTag(tag) << ".html";
    return outputDirectory() / name.str();
}

void writeJsonString(std::ostream& out, const std::string& value) {
    out << '"';
    for (unsigned char ch : value) {
        switch (ch) {
        case '\\':
            out << "\\\\";
            break;
        case '"':
            out << "\\\"";
            break;
        case '\n':
            out << "\\n";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\t':
            out << "\\t";
            break;
        // The JSON is embedded in an inline <script> block, so escape HTML
        // metacharacters too; a raw "</script>" inside a label would otherwise
        // terminate the script element early.
        case '<':
            out << "\\u003c";
            break;
        case '>':
            out << "\\u003e";
            break;
        case '&':
            out << "\\u0026";
            break;
        default:
            if (ch < 0x20) {
                out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(ch) << std::dec
                    << std::setfill(' ');
            } else {
                out << static_cast<char>(ch);
            }
            break;
        }
    }
    out << '"';
}

void writeHtmlText(std::ostream& out, const std::string& value) {
    for (char ch : value) {
        switch (ch) {
        case '&':
            out << "&amp;";
            break;
        case '<':
            out << "&lt;";
            break;
        case '>':
            out << "&gt;";
            break;
        case '"':
            out << "&quot;";
            break;
        default:
            out << ch;
            break;
        }
    }
}

void appendBaseEdges(
    const Mesh& mesh, const std::vector<std::pair<int, int>>& meshEdges, UseCase useCase, std::vector<RenderEdge>& edges
) {
    for (const auto& [a, b] : meshEdges) {
        if (validEdge(mesh, a, b)) {
            const PaletteEntry& entry = palette(useCase);
            edges.push_back({a, b, useCase, {}, entry.width});
        }
    }
}

void appendOverlays(const Mesh& mesh, const std::vector<EdgeOverlay>& overlays, std::vector<RenderEdge>& edges) {
    for (const EdgeOverlay& overlay : overlays) {
        if (!validEdge(mesh, overlay.a, overlay.b)) {
            continue;
        }
        const PaletteEntry& entry = palette(overlay.useCase);
        edges.push_back({overlay.a, overlay.b, overlay.useCase, overlay.label, entry.width});
    }
}

Vec3 meshCenter(const Mesh& mesh) {
    if (mesh.vertices.empty()) {
        return Vec3::Zero();
    }
    return (mesh.bboxMin() + mesh.bboxMax()) * 0.5;
}

void appendTranslatedMesh(Mesh& combined, const Mesh& source, const Vec3& center, const Vec3& offset) {
    const int vertexOffset = static_cast<int>(combined.vertices.size());
    combined.vertices.reserve(combined.vertices.size() + source.vertices.size());
    for (const Vec3& p : source.vertices) {
        combined.vertices.push_back(p - center + offset);
    }

    combined.faces.reserve(combined.faces.size() + source.faces.size());
    for (const Face& sourceFace : source.faces) {
        Face face = sourceFace;
        for (int& id : face.v) {
            id += vertexOffset;
        }
        combined.faces.push_back(face);
    }
}

void appendMeshEdges(
    const Mesh& mesh,
    const std::vector<std::pair<int, int>>& meshEdges,
    int vertexOffset,
    UseCase useCase,
    std::vector<RenderEdge>& edges,
    const char* firstLabel = nullptr
) {
    bool labeled = false;
    for (const auto& [a, b] : meshEdges) {
        if (!validEdge(mesh, a, b)) {
            continue;
        }
        const PaletteEntry& entry = palette(useCase);
        std::string label;
        if (!labeled && firstLabel) {
            label = firstLabel;
            labeled = true;
        }
        edges.push_back({a + vertexOffset, b + vertexOffset, useCase, label, entry.width});
    }
}

UseCase useCaseForFeatureEdge(const feature::FeatureGraphEdge& edge) {
    if (edge.removedByCleanup) {
        return UseCase::Warning;
    }
    if (edge.nonManifold) {
        return UseCase::Error;
    }
    if (edge.boundary) {
        return UseCase::Boundary;
    }
    if (edge.normalTensor && !edge.dihedral) {
        return UseCase::WeakFeature;
    }
    if (edge.cleanupBridge) {
        return UseCase::Candidate;
    }
    return UseCase::Feature;
}

std::vector<EdgeOverlay> featureOverlays(const Mesh& mesh, const feature::FeatureAnalysis& analysis) {
    std::vector<EdgeOverlay> overlays;
    overlays.reserve(analysis.graph.edges.size() + analysis.loops.size() * 8);

    for (const feature::FeatureGraphEdge& edge : analysis.graph.edges) {
        if (validEdge(mesh, edge.a, edge.b)) {
            overlays.push_back({edge.a, edge.b, useCaseForFeatureEdge(edge), {}});
        }
    }

    for (const feature::FeatureLoop& loop : analysis.loops) {
        if (loop.vertices.size() < 2) {
            continue;
        }
        for (std::size_t i = 0; i + 1 < loop.vertices.size(); ++i) {
            const int a = loop.vertices[i];
            const int b = loop.vertices[i + 1];
            if (validEdge(mesh, a, b)) {
                overlays.push_back({a, b, UseCase::FeatureLoop, {}});
            }
        }
        if (loop.closed && loop.vertices.size() > 2) {
            const int a = loop.vertices.back();
            const int b = loop.vertices.front();
            if (validEdge(mesh, a, b)) {
                overlays.push_back({a, b, UseCase::FeatureLoop, {}});
            }
        }
    }

    return overlays;
}

void writeHtml(
    const std::filesystem::path& path,
    const char* tag,
    const Mesh& mesh,
    const std::vector<RenderEdge>& edges,
    const std::vector<std::string>& summaryLines = {}
) {
    if (path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
    }

    std::ofstream out(path);
    if (!out) {
        std::cerr << "manumesh debugUtil: failed to write " << path.string() << "\n";
        return;
    }

    std::array<int, kPalette.size()> counts{};
    for (const RenderEdge& edge : edges) {
        for (std::size_t i = 0; i < kPalette.size(); ++i) {
            if (kPalette[i].useCase == edge.useCase) {
                ++counts[i];
                break;
            }
        }
    }

    out << std::setprecision(17);
    out << "<!doctype html>\n<html><head><meta charset=\"utf-8\">\n";
    out << "<title>ManuMesh debugUtil - " << sanitizeTag(tag) << "</title>\n";
    out << "<style>\n";
    out << "html,body{margin:0;height:100%;overflow:hidden;background:#111827;"
           "color:#e5e7eb;font:13px/1.4 system-ui,Segoe UI,sans-serif;}\n";
    out << "#view{display:block;width:100vw;height:100vh;}\n";
    out << "#panel{position:fixed;left:12px;top:12px;background:rgba(17,24,39,.82);"
           "border:1px solid #374151;border-radius:6px;padding:10px 12px;}\n";
    out << "#title{font-weight:600;margin-bottom:6px;}\n";
    out << ".row{display:flex;align-items:center;gap:8px;white-space:nowrap;}\n";
    out << ".sw{width:20px;height:3px;border-radius:2px;display:inline-block;}\n";
    out << "#hint{color:#9ca3af;margin-top:8px;}\n";
    out << "</style></head><body>\n<canvas id=\"view\"></canvas>\n";
    out << "<div id=\"panel\"><div id=\"title\">debugUtil: ";
    out << sanitizeTag(tag);
    out << "</div><div>vertices: " << mesh.vertices.size() << " / faces: " << mesh.faces.size()
        << " / lines: " << edges.size() << "</div>";
    for (const std::string& line : summaryLines) {
        out << "<div>";
        writeHtmlText(out, line);
        out << "</div>";
    }
    for (std::size_t i = 0; i < kPalette.size(); ++i) {
        if (counts[i] == 0) {
            continue;
        }
        out << "<div class=\"row\"><span class=\"sw\" style=\"background:" << kPalette[i].color << "\"></span><span>"
            << kPalette[i].name << ": " << counts[i] << "</span></div>";
    }
    out << "<div id=\"hint\">drag rotate, wheel zoom, double click reset</div></div>\n";

    out << "<script>\n";
    out << "const vertices=[";
    for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
        const Vec3& p = mesh.vertices[i];
        if (i > 0) {
            out << ",";
        }
        out << "[" << p.x() << "," << p.y() << "," << p.z() << "]";
    }
    out << "];\n";
    out << "const edges=[";
    for (std::size_t i = 0; i < edges.size(); ++i) {
        const RenderEdge& edge = edges[i];
        const PaletteEntry& entry = palette(edge.useCase);
        if (i > 0) {
            out << ",";
        }
        out << "{a:" << edge.a << ",b:" << edge.b << ",color:";
        writeJsonString(out, entry.color);
        out << ",width:" << edge.width << ",label:";
        writeJsonString(out, edge.label);
        out << "}";
    }
    out << "];\n";

    out << R"JS(
const canvas=document.getElementById('view');
const ctx=canvas.getContext('2d');
let rx=-0.55, ry=0.75, zoom=1.0, dragging=false, lx=0, ly=0;
function resize(){canvas.width=innerWidth*devicePixelRatio;canvas.height=innerHeight*devicePixelRatio;render();}
addEventListener('resize',resize);
let lo=[Infinity,Infinity,Infinity], hi=[-Infinity,-Infinity,-Infinity];
for(const p of vertices){for(let i=0;i<3;i++){lo[i]=Math.min(lo[i],p[i]);hi[i]=Math.max(hi[i],p[i]);}}
const center=[(lo[0]+hi[0])/2,(lo[1]+hi[1])/2,(lo[2]+hi[2])/2];
const diag=Math.max(1e-12, Math.hypot(hi[0]-lo[0],hi[1]-lo[1],hi[2]-lo[2]));
function project(p){
  let x=p[0]-center[0], y=p[1]-center[1], z=p[2]-center[2];
  const cy=Math.cos(ry), sy=Math.sin(ry), cx=Math.cos(rx), sx=Math.sin(rx);
  let x1=cy*x+sy*z, z1=-sy*x+cy*z;
  let y1=cx*y-sx*z1, z2=sx*y+cx*z1;
  const s=0.82*Math.min(canvas.width,canvas.height)*zoom/diag;
  const persp=1/(1+z2/Math.max(diag*3,1e-12));
  return [canvas.width/2+x1*s*persp, canvas.height/2-y1*s*persp];
}
function render(){
  ctx.setTransform(1,0,0,1,0,0);
  ctx.clearRect(0,0,canvas.width,canvas.height);
  ctx.lineCap='round';
  ctx.lineJoin='round';
  const pts=vertices.map(project);
  for(const e of edges){
    const a=pts[e.a], b=pts[e.b];
    if(!a||!b) continue;
    ctx.strokeStyle=e.color;
    ctx.globalAlpha=e.width <= 1.1 ? 0.46 : 0.92;
    ctx.lineWidth=Math.max(1,e.width)*devicePixelRatio;
    ctx.beginPath();
    ctx.moveTo(a[0],a[1]);
    ctx.lineTo(b[0],b[1]);
    ctx.stroke();
  }
  ctx.globalAlpha=1;
  ctx.fillStyle='#f9fafb';
  ctx.font=`${12*devicePixelRatio}px system-ui,Segoe UI,sans-serif`;
  for(const e of edges){
    if(!e.label) continue;
    const a=pts[e.a], b=pts[e.b];
    if(!a||!b) continue;
    ctx.fillText(e.label,(a[0]+b[0])/2+6*devicePixelRatio,(a[1]+b[1])/2-6*devicePixelRatio);
  }
}
canvas.addEventListener('pointerdown',e=>{dragging=true;lx=e.clientX;ly=e.clientY;canvas.setPointerCapture(e.pointerId);});
canvas.addEventListener('pointerup',()=>{dragging=false;});
canvas.addEventListener('pointermove',e=>{if(!dragging)return;ry+=(e.clientX-lx)*0.01;rx+=(e.clientY-ly)*0.01;lx=e.clientX;ly=e.clientY;render();});
canvas.addEventListener('wheel',e=>{e.preventDefault();zoom*=Math.exp(-e.deltaY*0.001);zoom=Math.max(0.05,Math.min(20,zoom));render();},{passive:false});
canvas.addEventListener('dblclick',()=>{rx=-0.55;ry=0.75;zoom=1;render();});
resize();
)JS";
    out << "</script>\n</body></html>\n";
}

bool envFlagEnabled(const char* name, bool defaultValue) {
    const char* raw = std::getenv(name);
    if (!raw || raw[0] == '\0') {
        return defaultValue;
    }

    std::string value(raw);
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value != "0" && value != "false" && value != "off" && value != "no";
}

#if !defined(_WIN32)
// Conservative allow-list for paths interpolated into a quoted shell command.
// Rejects anything that could expand or terminate the quoting ($, backticks,
// ;, quotes, backslashes, ...).
bool shellSafePath(const std::string& path) {
    for (char ch : path) {
        const auto uch = static_cast<unsigned char>(ch);
        if (std::isalnum(uch)) {
            continue;
        }
        switch (ch) {
        case '/':
        case '.':
        case '-':
        case '_':
        case '+':
        case ',':
        case '~':
        case ':':
        case '@':
        case ' ':
            continue;
        default:
            return false;
        }
    }
    return true;
}
#endif

// Maximum number of lines written into one HTML snapshot. Large meshes are
// uniformly sampled down to this cap so debug output stays loadable. Override
// with MANUMESH_DEBUG_UTIL_MAX_EDGES; values <= 0 disable the cap.
std::size_t maxRenderEdges() {
    constexpr long kDefaultMaxEdges = 200000;
    long limit = kDefaultMaxEdges;
    if (const char* raw = std::getenv("MANUMESH_DEBUG_UTIL_MAX_EDGES")) {
        if (raw[0] != '\0') {
            char* end = nullptr;
            const long parsed = std::strtol(raw, &end, 10);
            if (end != raw && *end == '\0') {
                limit = parsed;
            }
        }
    }
    if (limit <= 0) {
        return std::numeric_limits<std::size_t>::max();
    }
    return static_cast<std::size_t>(limit);
}

void capEdgesForOutput(std::vector<RenderEdge>& edges, std::vector<std::string>& summaryLines) {
    const std::size_t limit = maxRenderEdges();
    if (edges.size() <= limit) {
        return;
    }
    const std::size_t total = edges.size();
    std::vector<RenderEdge> sampled;
    sampled.reserve(limit);
    for (std::size_t i = 0; i < limit; ++i) {
        const auto index = static_cast<std::size_t>(static_cast<unsigned long long>(i) * total / limit);
        sampled.push_back(std::move(edges[index]));
    }
    edges = std::move(sampled);
    summaryLines.push_back(
        "sampled " + std::to_string(limit) + " of " + std::to_string(total) +
        " edges (override with MANUMESH_DEBUG_UTIL_MAX_EDGES)"
    );
}

void openBrowser(const std::filesystem::path& path) {
    if (!envFlagEnabled("MANUMESH_DEBUG_UTIL_OPEN", true)) {
        return;
    }

#if defined(_WIN32)
    // ShellExecuteW takes the document path verbatim, so no shell command line
    // is built and the path cannot inject commands. shell32 is loaded lazily to
    // avoid a hard link dependency for this debug-only utility.
    using ShellExecuteWFn = HINSTANCE(WINAPI*)(HWND, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, INT);
    const HMODULE shell32 = ::LoadLibraryW(L"shell32.dll");
    if (!shell32) {
        std::cerr << "manumesh debugUtil: cannot load shell32.dll to open the browser; open " << path.string()
                  << " manually\n";
        return;
    }
    const auto shellExecuteW =
        reinterpret_cast<ShellExecuteWFn>(reinterpret_cast<void*>(::GetProcAddress(shell32, "ShellExecuteW")));
    if (shellExecuteW) {
        (void)shellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    } else {
        std::cerr << "manumesh debugUtil: ShellExecuteW unavailable; open " << path.string() << " manually\n";
    }
    ::FreeLibrary(shell32);
#else
    const std::string pathString = path.string();
    // The command below goes through std::system, so only allow paths made of
    // characters that cannot break out of the double quotes or expand.
    if (!shellSafePath(pathString)) {
        std::cerr << "manumesh debugUtil: output path contains shell metacharacters; open " << pathString
                  << " manually\n";
        return;
    }
#if defined(__APPLE__)
    const std::string command = "open \"" + pathString + "\"";
#else
    const std::string command = "xdg-open \"" + pathString + "\"";
#endif
    (void)std::system(command.c_str());
#endif
}

void render(const char* tag, const Mesh& mesh, const std::vector<EdgeOverlay>& overlays, UseCase baseUseCase) {
    const std::vector<std::pair<int, int>> meshEdges = uniqueEdges(mesh);
    std::vector<RenderEdge> edges;
    edges.reserve(meshEdges.size() + overlays.size());
    appendBaseEdges(mesh, meshEdges, baseUseCase, edges);
    appendOverlays(mesh, overlays, edges);

    std::vector<std::string> summaryLines;
    capEdgesForOutput(edges, summaryLines);

    const std::filesystem::path path = makeOutputPath(tag);
    writeHtml(path, tag, mesh, edges, summaryLines);
    std::cerr << "manumesh debugUtil: " << path.string() << "\n";
    openBrowser(path);
}

void renderBeforeAfter(const char* tag, const Mesh& before, const Mesh& after) {
    Mesh combined;
    const Vec3 beforeCenter = meshCenter(before);
    const Vec3 afterCenter = meshCenter(after);
    const double scale = std::max({before.bboxDiag(), after.bboxDiag(), 1.0});
    // Translate copies into one temporary mesh so the same canvas controls can
    // compare both states without adding a second renderer path.
    const Vec3 beforeOffset(-0.75 * scale, 0.0, 0.0);
    const Vec3 afterOffset(0.75 * scale, 0.0, 0.0);

    appendTranslatedMesh(combined, before, beforeCenter, beforeOffset);
    const int afterVertexOffset = static_cast<int>(combined.vertices.size());
    appendTranslatedMesh(combined, after, afterCenter, afterOffset);

    std::vector<RenderEdge> edges;
    const std::vector<std::pair<int, int>> beforeEdges = uniqueEdges(before);
    const std::vector<std::pair<int, int>> afterEdges = uniqueEdges(after);
    edges.reserve(beforeEdges.size() + afterEdges.size());
    appendMeshEdges(before, beforeEdges, 0, UseCase::Mesh, edges, "before");
    appendMeshEdges(after, afterEdges, afterVertexOffset, UseCase::Accepted, edges, "after");

    std::vector<std::string> summaryLines;
    summaryLines.push_back(
        "before: vertices=" + std::to_string(before.vertices.size()) + " faces=" + std::to_string(before.faces.size()) +
        " edges=" + std::to_string(beforeEdges.size())
    );
    summaryLines.push_back(
        "after: vertices=" + std::to_string(after.vertices.size()) + " faces=" + std::to_string(after.faces.size()) +
        " edges=" + std::to_string(afterEdges.size())
    );
    capEdgesForOutput(edges, summaryLines);

    const std::filesystem::path path = makeOutputPath(tag);
    writeHtml(path, tag, combined, edges, summaryLines);
    std::cerr << "manumesh debugUtil: " << path.string() << "\n";
    openBrowser(path);
}

} // namespace

void showWireframe(const char* tag, const Mesh& mesh, UseCase useCase) { render(tag, mesh, {}, useCase); }

void showEdge(const char* tag, const Mesh& mesh, int a, int b, UseCase useCase, const char* label) {
    std::vector<EdgeOverlay> overlays;
    overlays.push_back({a, b, useCase, label ? label : ""});
    render(tag, mesh, overlays, UseCase::Mesh);
}

void showEdges(const char* tag, const Mesh& mesh, const std::vector<EdgeOverlay>& overlays, UseCase baseUseCase) {
    render(tag, mesh, overlays, baseUseCase);
}

void showFeatures(const char* tag, const Mesh& mesh, const feature::FeatureAnalysis& analysis) {
    render(tag, mesh, featureOverlays(mesh, analysis), UseCase::Mesh);
}

void showBeforeAfter(const char* tag, const Mesh& before, const Mesh& after) { renderBeforeAfter(tag, before, after); }

} // namespace manumesh::debugUtil

#endif
