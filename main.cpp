#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cmath>
#include <map>
#include <algorithm>
#include <memory>
#include <commdlg.h>

using namespace std;

#define IDC_EDIT_INPUT 101
#define IDC_BTN_SAVE   102

const int BASE_FONT_SIZE = 28;
const int RENDER_X = 50;
const int RENDER_Y = 100;

class IRenderer {
public:
    virtual void SetFontSize(int size) = 0;
    virtual void SetFontStyle(bool italic) = 0;
    virtual void DrawLine(int x1, int y1, int x2, int y2) = 0;
    virtual void DrawTextStr(int x, int y, const wstring& text) = 0;
    virtual int GetTextWidth(const wstring& text) = 0;
    virtual int GetTextHeight() = 0;
    virtual ~IRenderer() {}
};

class GDIRenderer : public IRenderer {
    HDC hdc;
    HFONT hFont;
    int currentFontSize;
    bool isItalic;
public:
    GDIRenderer(HDC h) : hdc(h), currentFontSize(BASE_FONT_SIZE), isItalic(false), hFont(NULL) {
        UpdateFont();
        SetBkMode(hdc, TRANSPARENT);
    }
    ~GDIRenderer() { if (hFont) DeleteObject(hFont); }

    void UpdateFont() {
        if (hFont) DeleteObject(hFont);
        hFont = CreateFont(currentFontSize, 0, 0, 0, FW_NORMAL, isItalic, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_ROMAN, L"Times New Roman");
        SelectObject(hdc, hFont);
    }

    void SetFontSize(int size) override {
        if (size == currentFontSize) return;
        currentFontSize = size;
        UpdateFont();
    }

    void SetFontStyle(bool italic) override {
        if (italic == isItalic) return;
        isItalic = italic;
        UpdateFont();
    }

    void DrawLine(int x1, int y1, int x2, int y2) override {
        MoveToEx(hdc, x1, y1, NULL);
        LineTo(hdc, x2, y2);
    }

    void DrawTextStr(int x, int y, const wstring& text) override {
        TextOut(hdc, x, y, text.c_str(), text.length());
    }

    int GetTextWidth(const wstring& text) override {
        SIZE size;
        GetTextExtentPoint32(hdc, text.c_str(), text.length(), &size);
        return size.cx;
    }

    int GetTextHeight() override {
        SIZE size;
        GetTextExtentPoint32(hdc, L"Tg", 2, &size);
        return size.cy;
    }
};

class SVGRenderer : public IRenderer {
    wstringstream buffer;
    int currentFontSize;
    bool isItalic;
public:
    SVGRenderer() : currentFontSize(BASE_FONT_SIZE), isItalic(false) {}

    wstring GetContent() { return buffer.str(); }

    void SetFontSize(int size) override { currentFontSize = size; }
    void SetFontStyle(bool italic) override { isItalic = italic; }

    void DrawLine(int x1, int y1, int x2, int y2) override {
        buffer << L"<line x1=\"" << x1 << L"\" y1=\"" << y1
               << L"\" x2=\"" << x2 << L"\" y2=\"" << y2
               << L"\" stroke=\"black\" stroke-width=\"1.5\" />\n";
    }

    void DrawTextStr(int x, int y, const wstring& text) override {
        int baselineY = y + (int)(currentFontSize * 0.8);
        buffer << L"<text x=\"" << x << L"\" y=\"" << baselineY
               << L"\" font-family=\"Times New Roman\" "
               << (isItalic ? L"font-style=\"italic\" " : L"font-style=\"normal\" ")
               << L"font-size=\"" << currentFontSize << L"\" dominant-baseline=\"auto\">" << text << L"</text>\n";
    }

    int GetTextWidth(const wstring& text) override {
        HDC hdc = GetDC(NULL);
        GDIRenderer gdi(hdc);
        gdi.SetFontSize(currentFontSize);
        gdi.SetFontStyle(isItalic);
        int w = gdi.GetTextWidth(text);
        ReleaseDC(NULL, hdc);
        return w;
    }

    int GetTextHeight() override {
        HDC hdc = GetDC(NULL);
        GDIRenderer gdi(hdc);
        gdi.SetFontSize(currentFontSize);
        int h = gdi.GetTextHeight();
        ReleaseDC(NULL, hdc);
        return h;
    }
};

struct MathNode {
    int width = 0, height = 0, ascent = 0;
    virtual ~MathNode() {}
    virtual void Measure(IRenderer* r) = 0;
    virtual void Draw(IRenderer* r, int x, int y) = 0;
};

struct RowNode : MathNode {
    vector<shared_ptr<MathNode>> children;
    void Add(shared_ptr<MathNode> node) { children.push_back(node); }

    void Measure(IRenderer* r) override {
        width = 0; height = 0; ascent = 0;
        int maxAscent = 0, maxDescent = 0;
        for (auto& c : children) {
            c->Measure(r);
            width += c->width;
            if (c->ascent > maxAscent) maxAscent = c->ascent;
            int descent = c->height - c->ascent;
            if (descent > maxDescent) maxDescent = descent;
        }
        ascent = maxAscent;
        height = maxAscent + maxDescent;
    }

    void Draw(IRenderer* r, int x, int y) override {
        int curX = x;
        for (auto& c : children) {
            c->Draw(r, curX, y + (ascent - c->ascent));
            curX += c->width;
        }
    }
};

struct TextNode : MathNode {
    wstring text;
    bool italic;
    int fontSize;
    int hOffset;

    TextNode(wstring t, bool it, int fs, int offset = 0) : text(t), italic(it), fontSize(fs), hOffset(offset) {}

    void Measure(IRenderer* r) override {
        r->SetFontSize(fontSize);
        r->SetFontStyle(italic);
        width = r->GetTextWidth(text) + hOffset;
        height = r->GetTextHeight();
        ascent = (int)(height * 0.8);
    }

    void Draw(IRenderer* r, int x, int y) override {
        r->SetFontSize(fontSize);
        r->SetFontStyle(italic);
        r->DrawTextStr(x + hOffset, y, text);
    }
};

struct FracNode : MathNode {
    shared_ptr<MathNode> num, den;
    int fontSize;

    FracNode(shared_ptr<MathNode> n, shared_ptr<MathNode> d, int fs)
        : num(n), den(d), fontSize(fs) {}

    void Measure(IRenderer* r) override {
        num->Measure(r); den->Measure(r);
        width = max(num->width, den->width) + 10;
        height = num->height + den->height + 4;
        ascent = num->height + 2;
    }

    void Draw(IRenderer* r, int x, int y) override {
        int midX = x + width / 2;
        num->Draw(r, midX - num->width / 2, y);
        int lineY = y + num->height + 2;
        r->DrawLine(x, lineY, x + width, lineY);
        den->Draw(r, midX - den->width / 2, lineY + 2);
    }
};

struct ScriptNode : MathNode {
    shared_ptr<MathNode> base, super, sub;

    ScriptNode(shared_ptr<MathNode> b) : base(b) {}

    void Measure(IRenderer* r) override {
        base->Measure(r);
        width = base->width;
        ascent = base->ascent;
        height = base->height;

        int scriptW = 0;
        if (super) {
            super->Measure(r);
            scriptW = max(scriptW, super->width);
            ascent = max(ascent, super->height + base->ascent / 2);
        }
        if (sub) {
            sub->Measure(r);
            scriptW = max(scriptW, sub->width);
            height = max(height, sub->height + ascent);
        }
        width += scriptW;
    }

    void Draw(IRenderer* r, int x, int y) override {
        base->Draw(r, x, y + (ascent - base->ascent));
        int scriptX = x + base->width;
        int baseY = y + (ascent - base->ascent);

        if (super) {
            super->Draw(r, scriptX, baseY - (int)(super->height * 0.5));
        }
        if (sub) {
            sub->Draw(r, scriptX, baseY + base->height - base->ascent + (int)(sub->height * 0.1));
        }
    }
};

struct BigOperatorNode : MathNode {
    wstring opSymbol;
    shared_ptr<MathNode> lower, upper;
    int fontSize;
    bool isTextOp;

    BigOperatorNode(wstring sym, int fs, bool textMode) : opSymbol(sym), fontSize(fs), isTextOp(textMode) {}

    void Measure(IRenderer* r) override {
        r->SetFontSize(isTextOp ? fontSize : (int)(fontSize * 1.5));
        r->SetFontStyle(false);
        int baseW = r->GetTextWidth(opSymbol);
        int baseH = r->GetTextHeight();

        int lowW = 0, lowH = 0;
        int upW = 0, upH = 0;

        if (lower) { lower->Measure(r); lowW = lower->width; lowH = lower->height; }
        if (upper) { upper->Measure(r); upW = upper->width; upH = upper->height; }

        width = max(baseW, max(lowW, upW)) + 4;

        int topPart = upH + (int)(baseH * 0.8);
        int botPart = (baseH - (int)(baseH * 0.8)) + lowH;

        ascent = topPart;
        height = topPart + botPart;
    }

    void Draw(IRenderer* r, int x, int y) override {
        int midX = x + width / 2;

        if (upper) {
            upper->Draw(r, midX - upper->width/2, y);
        }

        r->SetFontSize(isTextOp ? fontSize : (int)(fontSize * 1.5));
        r->SetFontStyle(false);
        int opW = r->GetTextWidth(opSymbol);
        int opH = r->GetTextHeight();

        int opY = y + (upper ? upper->height : 0);
        r->DrawTextStr(midX - opW/2, opY, opSymbol);

        if (lower) {
            lower->Draw(r, midX - lower->width/2, opY + opH);
        }
    }
};

struct IntegralNode : MathNode {
    shared_ptr<MathNode> lower, upper, integrand;
    int baseSize;

    IntegralNode(int size) : baseSize(size) {}

    void Measure(IRenderer* r) override {
        r->SetFontSize((int)(baseSize * 1.5));
        r->SetFontStyle(false);
        int intW = r->GetTextWidth(L"\u222B");
        int intH = r->GetTextHeight();

        int limitsH = 0;
        int limitsW = 0;

        if (upper) { upper->Measure(r); limitsW = max(limitsW, upper->width); limitsH += upper->height; }
        if (lower) { lower->Measure(r); limitsW = max(limitsW, lower->width); limitsH += lower->height; }

        width = intW + limitsW + 4;
        height = max(intH, limitsH);
        ascent = intH/2;
    }

    void Draw(IRenderer* r, int x, int y) override {
        r->SetFontSize((int)(baseSize * 1.5));
        r->SetFontStyle(false);
        int intW = r->GetTextWidth(L"\u222B");
        int intH = r->GetTextHeight();

        int opY = y + (ascent - intH/2);
        r->DrawTextStr(x, opY, L"\u222B");

        int limX = x + intW + 2;

        if (upper) upper->Draw(r, limX, opY + 2);

        if (lower) lower->Draw(r, limX, opY + intH - lower->height - 2);
    }
};

struct SqrtNode : MathNode {
    shared_ptr<MathNode> child;
    shared_ptr<MathNode> index;

    SqrtNode(shared_ptr<MathNode> c, shared_ptr<MathNode> idx = nullptr) : child(c), index(idx) {}

    void Measure(IRenderer* r) override {
        child->Measure(r);
        width = child->width + 15;
        height = child->height + 5;
        ascent = child->ascent + 5;
        if (index) {
            index->Measure(r);
            width += max(0, index->width - 5);
            ascent = max(ascent, index->height + 5);
            height = max(height, ascent + (child->height - child->ascent));
        }
    }

    void Draw(IRenderer* r, int x, int y) override {
        int startX = x;
        if (index) {
            index->Draw(r, x, y);
            startX += max(5, index->width);
        }

        child->Draw(r, startX + 10, y + (ascent - child->ascent - 5) + 5);

        int bottomY = y + ascent + (child->height - child->ascent);
        int topY = y + (ascent - child->ascent - 5);

        r->DrawLine(startX, bottomY - (bottomY-topY)/2, startX + 5, bottomY);
        r->DrawLine(startX + 5, bottomY, startX + 10, topY);
        r->DrawLine(startX + 10, topY, startX + child->width + 15, topY);
    }
};

struct ScalingFenceNode : MathNode {
    shared_ptr<MathNode> content;
    wstring left, right;

    ScalingFenceNode(shared_ptr<MathNode> c, wstring l, wstring r) : content(c), left(l), right(r) {}

    void Measure(IRenderer* r) override {
        content->Measure(r);
        width = content->width + 14;
        height = content->height;
        ascent = content->ascent;
    }

    void Draw(IRenderer* r, int x, int y) override {
        content->Draw(r, x + 7, y);
        int h = height;

        if (left == L"|") {
            r->DrawLine(x + 2, y, x + 2, y + h);
        } else if (left == L"(") {
            r->DrawLine(x + 5, y, x + 1, y + h/2);
            r->DrawLine(x + 1, y + h/2, x + 5, y + h);
        } else if (left == L"[") {
            r->DrawLine(x + 5, y, x + 5, y + h);
            r->DrawLine(x + 5, y, x + 10, y);
            r->DrawLine(x + 5, y + h, x + 10, y + h);
        }

        if (right == L"|") {
            r->DrawLine(x + width - 2, y, x + width - 2, y + h);
        } else if (right == L")") {
            r->DrawLine(x + width - 5, y, x + width - 1, y + h/2);
            r->DrawLine(x + width - 1, y + h/2, x + width - 5, y + h);
        } else if (right == L"]") {
            r->DrawLine(x + width - 5, y, x + width - 5, y + h);
            r->DrawLine(x + width - 5, y, x + width - 10, y);
            r->DrawLine(x + width - 5, y + h, x + width - 10, y + h);
        }
    }
};

class Parser {
    wstring source;
    size_t pos;
    int currentFontSize;
    static map<wstring, wstring> symbols;

    wchar_t Peek() { if (pos < source.length()) return source[pos]; return 0; }
    wchar_t Next() { if (pos < source.length()) return source[pos++]; return 0; }

public:
    Parser(const wstring& s, int fs) : source(s), pos(0), currentFontSize(fs) {
        InitSymbols();
    }

    shared_ptr<RowNode> Parse() {
        auto row = make_shared<RowNode>();
        while (pos < source.length()) {
            wchar_t c = Peek();
            if (c == L'}' || c == L']') break;

            auto node = ParseItem();
            if (node) {
                node = CheckForScripts(node);
                row->Add(node);
            }
        }
        return row;
    }

private:
    static void InitSymbols() {
        if (!symbols.empty()) return;
        symbols[L"Alpha"] = L"\u0391"; symbols[L"Beta"] = L"\u0392"; symbols[L"Gamma"] = L"\u0393";
        symbols[L"Delta"] = L"\u0394"; symbols[L"Epsilon"] = L"\u0395"; symbols[L"Zeta"] = L"\u0396";
        symbols[L"Eta"] = L"\u0397"; symbols[L"Theta"] = L"\u0398"; symbols[L"Lambda"] = L"\u039B";
        symbols[L"Xi"] = L"\u039E"; symbols[L"Pi"] = L"\u03A0"; symbols[L"Sigma"] = L"\u03A3";
        symbols[L"Phi"] = L"\u03A6"; symbols[L"Psi"] = L"\u03A8"; symbols[L"Omega"] = L"\u03A9";

        symbols[L"alpha"] = L"\u03B1"; symbols[L"beta"] = L"\u03B2"; symbols[L"gamma"] = L"\u03B3";
        symbols[L"delta"] = L"\u03B4"; symbols[L"epsilon"] = L"\u03B5"; symbols[L"zeta"] = L"\u03B6";
        symbols[L"eta"] = L"\u03B7"; symbols[L"theta"] = L"\u03B8"; symbols[L"lambda"] = L"\u03BB";
        symbols[L"xi"] = L"\u03BE"; symbols[L"pi"] = L"\u03C0"; symbols[L"sigma"] = L"\u03C3";
        symbols[L"phi"] = L"\u03C6"; symbols[L"psi"] = L"\u03C8"; symbols[L"omega"] = L"\u03C9";

        symbols[L"infty"] = L"\u221E"; symbols[L"approx"] = L"\u2248"; symbols[L"neq"] = L"\u2260";
        symbols[L"le"] = L"\u2264"; symbols[L"ge"] = L"\u2265"; symbols[L"pm"] = L"\u00B1";
        symbols[L"cdot"] = L"\u2219";
        symbols[L"to"] = L"\u2192";
        symbols[L"thinspace"] = L" ";
        symbols[L"quad"] = L"  ";
        symbols[L"!"] = L"";
        symbols[L","] = L"";
        symbols[L"'"] = L"'";
    }

    shared_ptr<MathNode> CheckForScripts(shared_ptr<MathNode> base) {
        auto bigOp = dynamic_pointer_cast<BigOperatorNode>(base);

        while (Peek() == L'^' || Peek() == L'_') {
            wchar_t type = Next();
            Parser pSub(L"", (int)(currentFontSize * 0.7));
            pSub.source = ParseBlockStr();
            auto scriptContent = pSub.Parse();

            if (bigOp) {
                if (type == L'^') bigOp->upper = scriptContent;
                else bigOp->lower = scriptContent;
            } else {
                auto script = dynamic_pointer_cast<ScriptNode>(base);
                if (!script) script = make_shared<ScriptNode>(base);
                if (type == L'^') script->super = scriptContent;
                else script->sub = scriptContent;
                base = script;
            }
        }
        return base;
    }

    wstring ParseBlockStr() {
        wstring res = L"";
        if (Peek() == L'{') {
            Next();
            int depth = 1;
            while (pos < source.length() && depth > 0) {
                wchar_t c = Next();
                if (c == L'{') depth++;
                else if (c == L'}') depth--;
                if (depth > 0) res += c;
            }
        } else if (Peek() == L'[') {
            Next();
            while (pos < source.length() && Peek() != L']') res += Next();
            if (pos < source.length()) Next();
        } else {
            if (pos < source.length()) res += Next();
        }
        return res;
    }

    shared_ptr<MathNode> ParseItem() {
        if (pos >= source.length()) return nullptr;
        wchar_t c = Peek();
        Next();

        if (c == L'\\') return ParseCommand();

        if (iswdigit(c)) {
            wstring num = L""; num += c;
            while (iswdigit(Peek()) || Peek() == L'.') num += Next();
            return make_shared<TextNode>(num, false, currentFontSize);
        }

        if (iswalpha(c)) {
            wstring var = L""; var += c;
            return make_shared<TextNode>(var, true, currentFontSize);
        }

        wstring op = L""; op += c;
        return make_shared<TextNode>(op, false, currentFontSize);
    }

    shared_ptr<MathNode> ParseCommand() {
        wstring cmd = L"";
        while (iswalpha(Peek())) cmd += Next();

        if (cmd == L"left") {
            wstring leftDelim = L"";
            if (Peek() != 0) leftDelim += Next();

            int startPos = pos;
            int depth = 0;
            while (pos < source.length()) {
                if (source.substr(pos, 5) == L"\\left") depth++;
                if (source.substr(pos, 6) == L"\\right") {
                    if (depth == 0) break;
                    depth--;
                }
                pos++;
            }

            wstring inner = source.substr(startPos, pos - startPos);
            Parser p(inner, currentFontSize);
            auto content = p.Parse();

            if (source.substr(pos, 6) == L"\\right") pos += 6;

            wstring rightDelim = L"";
            if (Peek() != 0) rightDelim += Next();

            if (rightDelim.length() == 0 && pos < source.length()) rightDelim += Next();

            return make_shared<ScalingFenceNode>(content, leftDelim, rightDelim);
        }

        if (cmd == L"frac") {
            Parser pN(ParseBlockStr(), currentFontSize);
            Parser pD(ParseBlockStr(), currentFontSize);
            return make_shared<FracNode>(pN.Parse(), pD.Parse(), currentFontSize);
        }
        if (cmd == L"sqrt") {
            shared_ptr<MathNode> index = nullptr;
            if (Peek() == L'[') {
                Parser pIdx(ParseBlockStr(), (int)(currentFontSize*0.6));
                index = pIdx.Parse();
            }
            Parser p(ParseBlockStr(), currentFontSize);
            return make_shared<SqrtNode>(p.Parse(), index);
        }

        if (cmd == L"int") return make_shared<IntegralNode>(currentFontSize);
        if (cmd == L"sum" || cmd == L"prod") return make_shared<BigOperatorNode>(L"\u2211", currentFontSize, false);
        if (cmd == L"lim") return make_shared<BigOperatorNode>(L"lim", currentFontSize, true);

        if (cmd == L"mathrm") {
            wstring content = ParseBlockStr();
            return make_shared<TextNode>(content, false, currentFontSize);
        }

        if (cmd == L"!") {
            return make_shared<TextNode>(L"", false, currentFontSize, (int)(-currentFontSize * 0.15));
        }
        if (cmd == L",") {
            return make_shared<TextNode>(L"", false, currentFontSize, (int)(currentFontSize * 0.15));
        }

        bool isFunction = (cmd == L"sin" || cmd == L"cos" || cmd == L"tan" ||
                           cmd == L"log" || cmd == L"ln" || cmd == L"lg" || cmd == L"exp" ||
                           cmd == L"sinh" || cmd == L"cosh" || cmd == L"asin" || cmd == L"acos");

        if (isFunction) {
            return make_shared<TextNode>(cmd, false, currentFontSize);
        }

        if (symbols.count(cmd)) {
            return make_shared<TextNode>(symbols[cmd], false, currentFontSize);
        }

        return make_shared<TextNode>(L"?", false, currentFontSize);
    }
};

map<wstring, wstring> Parser::symbols;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

wstring currentFormula = L"";

void SaveToSVG(HWND hwnd) {
    wchar_t szFile[MAX_PATH] = L"formula.svg";
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
    ofn.lpstrFilter = L"SVG Files (*.svg)\0*.svg\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

    if (!GetSaveFileName(&ofn)) {
        return;
    }

    SVGRenderer svg;
    svg.SetFontSize(BASE_FONT_SIZE);
    Parser parser(currentFormula, BASE_FONT_SIZE);
    auto root = parser.Parse();

    root->Measure(&svg);

    wstringstream fullFile;
    int padding = 50;
    int totalWidth = root->width + padding * 2;
    int totalHeight = root->height + padding * 2 + 20;

    fullFile << L"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << totalWidth
             << L"\" height=\"" << totalHeight << L"\" viewBox=\"0 0 "
             << totalWidth << L" " << totalHeight << L"\">\n";
    fullFile << L"<rect width=\"100%\" height=\"100%\" fill=\"white\" />\n";

    root->Draw(&svg, padding, padding + root->ascent);
    fullFile << svg.GetContent() << L"</svg>";

    HANDLE hFile = CreateFile(ofn.lpstrFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile != INVALID_HANDLE_VALUE) {
        string utf8;
        wstring content = fullFile.str();
        int size = WideCharToMultiByte(CP_UTF8, 0, content.c_str(), -1, NULL, 0, NULL, NULL);
        utf8.resize(size);
        WideCharToMultiByte(CP_UTF8, 0, content.c_str(), -1, &utf8[0], size, NULL, NULL);

        DWORD written;
        WriteFile(hFile, utf8.c_str(), utf8.size() - 1, &written, NULL);
        CloseHandle(hFile);

        wstring successMsg = L"File saved: ";
        successMsg += ofn.lpstrFile;
        MessageBox(hwnd, successMsg.c_str(), L"Saved", MB_OK);
    } else {
        MessageBox(hwnd, L"Error saving file.", L"Error", MB_ICONERROR | MB_OK);
    }
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    _wsetlocale(LC_ALL, L"");

    const wchar_t CLASS_NAME[] = L"MathSuite";
    WNDCLASS wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClass(&wc);

    const DWORD WINDOW_STYLE = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    HWND hwnd = CreateWindowEx(0, CLASS_NAME, L"Math Editor Complete", WINDOW_STYLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 1000, 600, NULL, NULL, hInstance, NULL);

    if (!hwnd) return 0;
    ShowWindow(hwnd, nCmdShow);
    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static HWND hEdit;
    switch (uMsg) {
    case WM_CREATE:
        CreateWindow(L"STATIC", L"LaTeX Input:", WS_CHILD | WS_VISIBLE, 10, 10, 100, 20, hwnd, NULL, NULL, NULL);
        hEdit = CreateWindowEx(0, L"EDIT", currentFormula.c_str(),
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 10, 30, 800, 30, hwnd, (HMENU)IDC_EDIT_INPUT, NULL, NULL);
        SendMessage(hEdit, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

        CreateWindow(L"BUTTON", L"Save SVG", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            820, 30, 100, 30, hwnd, (HMENU)IDC_BTN_SAVE, NULL, NULL);

        CreateWindow(L"STATIC", L"Output:", WS_CHILD | WS_VISIBLE, 10, 70, 100, 20, hwnd, NULL, NULL, NULL);
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_BTN_SAVE) {
            SaveToSVG(hwnd);
        }
        else if (LOWORD(wParam) == IDC_EDIT_INPUT && HIWORD(wParam) == EN_CHANGE) {
            int len = GetWindowTextLength(hEdit);
            vector<wchar_t> buf(len + 1);
            GetWindowText(hEdit, &buf[0], len + 1);
            currentFormula = &buf[0];
            InvalidateRect(hwnd, NULL, TRUE);
        }
        break;

    case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            GDIRenderer gdi(hdc);
            Parser parser(currentFormula, BASE_FONT_SIZE);
            auto root = parser.Parse();

            root->Measure(&gdi);
            root->Draw(&gdi, RENDER_X, RENDER_Y + root->ascent);

            EndPaint(hwnd, &ps);
        }
        return 0;

    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE h, HINSTANCE p, LPSTR c, int n) {
    return wWinMain(h, nullptr, nullptr, n);
}
