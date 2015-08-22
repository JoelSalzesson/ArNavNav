#include "Mesh.h"
#include "Except.h"
#include <map>
#include <queue>

typedef pair<Vertex*, Vertex*> VPair;

// map (from,to) -> halfedge
map<VPair, HalfEdge*> unpaired;

HalfEdge* seekPair(Vertex* v1, Vertex* v2, HalfEdge* add) {
    VPair ko(v2, v1);
    auto it = unpaired.find(ko);
    if (it != unpaired.end()) {
        HalfEdge* p = it->second;
        unpaired.erase(it);
        p->opposite = add;
        add->opposite = p;
    }
    VPair ks(v1, v2);
    if (unpaired.find(ks) != unpaired.end())
        throw Exception("Not all triangles are clockwise");
    unpaired[ks] = add;
    return nullptr;
}


void Mesh::connectTri()
{
    unpaired.clear();

    for(auto t: m_tri) 
    {
        HalfEdge* h0 = new HalfEdge();
        h0->tri = t;
        h0->from = t->v[0];
        h0->to = t->v[1];
        h0->midPnt = (t->v[1]->p + t->v[0]->p) * 0.5f;
        t->h[0] = h0;

        HalfEdge* h1 = new HalfEdge();
        h1->tri = t;
        h1->from = t->v[1];
        h1->to = t->v[2];
        h1->midPnt = (t->v[2]->p + t->v[1]->p) * 0.5f;
        t->h[1] = h1;

        HalfEdge* h2 = new HalfEdge();
        h2->tri = t;
        h2->from = t->v[2];
        h2->to = t->v[0];
        h2->midPnt = (t->v[0]->p + t->v[2]->p) * 0.5f;
        t->h[2] = h2;

        h0->next = h1;
        h1->next = h2;
        h2->next = h0;

        seekPair(t->v[0], t->v[1], h0);
        seekPair(t->v[1], t->v[2], h1);
        seekPair(t->v[2], t->v[0], h2);
    }

    // go over half edges, create triangles links
    for (auto t : m_tri)
    {
        for(int i = 0; i < 3; ++i) {
            if (t->h[i]->opposite != nullptr)
                t->nei[i] = t->h[i]->opposite->tri;
        }
    }

}


static float sign(const Vec2& p1, const Vec2& p2, const Vec2& p3)
{
    return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
}

static bool isPointInTri(const Vec2& pt, Triangle* t)//const Vec2& v1, const Vec2& v2, const Vec2& v3)
{
    bool b1, b2, b3;

    b1 = sign(pt, t->v[0]->p, t->v[1]->p) < 0.0f;
    b2 = sign(pt, t->v[1]->p, t->v[2]->p) < 0.0f;
    b3 = sign(pt, t->v[2]->p, t->v[0]->p) < 0.0f;

    return ((b1 == b2) && (b2 == b3));
}


Triangle* Mesh::findContaining(const Vec2& p)
{
    for(auto t: m_tri) {
        if (isPointInTri(p, t))
            return t;
    }
    return nullptr;
}

struct PrioNode
{
    PrioNode(HalfEdge* _h, float _p) :h(_h), prio(_p) {}
    HalfEdge* h;
    float prio = 0.0f;
};

bool lessPrioNode(const PrioNode& a, const PrioNode& b) {
    return a.prio > b.prio;
}

float distm(const Vec2& a, const Vec2& b) {
    // can't use distSq since its not linear so it makes the heuristic bad
    return std::sqrt(distSq(a, b));
}

bool Mesh::edgesAstarSearch(const Vec2& startPos, const Vec2& endPos, Triangle* start, Triangle* end, vector<Triangle*>& corridor)
{
    if (start == end)
        return false;
    priority_queue<PrioNode, vector<PrioNode>, decltype(lessPrioNode)*> tq(lessPrioNode);
    HalfEdge* dummy = (HalfEdge*)0xff;
    vector<HalfEdge*> destEdges;
    vector<float> destCost; // used when selecting the best dest reached

    for(int i = 0; i < 3; ++i) 
    {
        auto sh = start->h[i];
        if (sh) {
            // fix mid point of start triangle to be closer to the real target
            sh->midPnt = project(startPos, sh->from->p, sh->to->p); // project to the line of the edge
            if (sh->opposite)
                sh->opposite->midPnt = sh->midPnt;
            destEdges.push_back(sh);
            destCost.push_back(FLT_MAX);
        }
        auto h = end->h[i]->opposite;
        if (h) {
            h->midPnt = project(endPos, h->from->p, h->to->p); // fix mid point of end triangle to be closer to the real target
            if (h->opposite)
                h->opposite->midPnt = h->midPnt;
            h->costSoFar = distm(endPos, h->midPnt);
            h->cameFrom = dummy;
            float heur = distm(h->midPnt, startPos);
            tq.push(PrioNode(h, h->costSoFar + heur));
        }
    }

    int destReached = 0;
    while (!tq.empty() ) 
    {
        PrioNode curn = tq.top();
        HalfEdge* cur = curn.h;
        tq.pop();    

        auto dsit = std::find(destEdges.begin(), destEdges.end(), cur);
        if (dsit != destEdges.end()) 
        {
            destCost[dsit - destEdges.begin()] = curn.prio;
            ++destReached;
            if (destReached > destEdges.size())
                break;
            continue; // need to find more ways to get there
        }

        HalfEdge* next[2] = { cur->next->opposite, cur->next->next->opposite }; 
        for(int i = 0; i < 2; ++i) 
        {
            HalfEdge* n = next[i];
            if (!n)
                continue;
            float costToThis = cur->costSoFar + distm(cur->midPnt, n->midPnt);
            if (costToThis > n->costSoFar)
                continue;
            n->costSoFar = costToThis;
            n->cameFrom = cur;
            float heur = n->costSoFar + distm(n->midPnt, startPos);
            tq.push(PrioNode(n, heur));
        }
    }
    if (destReached == 0)
        return false;

    auto dit = min_element(destCost.begin(), destCost.end());
    HalfEdge *h = destEdges[dit - destCost.begin()];

    // make it in reverse order
    while (h != dummy) {
        corridor.push_back(h->tri);
        h = h->cameFrom;
    }
    // the end triangle doesn't have any halfedges that are part of the the corridor so just add it
    corridor.push_back(end);
    return true;
}



inline float triarea2(const Vec2* a, const Vec2* b, const Vec2* c)
{
    const float ax = b->x - a->x;
    const float ay = b->y - a->y;
    const float bx = c->x - a->x;
    const float by = c->y - a->y;
    return bx*ay - ax*by;
}

float vdistsqr(const Vec2* a, const Vec2* b)
{
    float dx = a->x - b->x;
    float dy = a->y - b->y;
    return dx*dx + dy*dy;
}

inline bool vequal(const Vec2* a, const Vec2* b)
{
    static const float eq = 0.001f*0.001f;
    return vdistsqr(a, b) < eq;
}

// http://digestingduck.blogspot.co.il/2010/03/simple-stupid-funnel-algorithm.html
// http://digestingduck.blogspot.co.il/2010/07/my-paris-game-ai-conference.html
void stringPull(vector<Vec2*> portalsRight, vector<Vec2*> portalsLeft, vector<Vec2>& output)
{

    // Init scan state
    int apexIndex = 0, leftIndex = 0, rightIndex = 0;
    auto *portalApex = portalsLeft[0];
    auto *portalLeft = portalsLeft[0];
    auto *portalRight = portalsRight[0];

    // Add start point.
    output.push_back(*portalApex);


    for (int i = 1; i < portalsRight.size(); ++i)
    {
        auto left = portalsLeft[i];
        auto right = portalsRight[i];

        // Update right vertex.
        if (triarea2(portalApex, portalRight, right) >= 0.0f)
        {
            if (vequal(portalApex, portalRight) || triarea2(portalApex, portalLeft, right) < 0.0f)
            {
                // Tighten the funnel.
                portalRight = right;
                rightIndex = i;
            }
            else
            {
                // Right over left, insert left to path and restart scan from portal left point.
                output.push_back(*portalLeft);

                // Make current left the new apex.
                portalApex = portalLeft;
                apexIndex = leftIndex;
                // Reset portal
                portalLeft = portalApex;
                portalRight = portalApex;
                leftIndex = apexIndex;
                rightIndex = apexIndex;
                // Restart scan
                i = apexIndex;
                continue;
            }
        }

        // Update left vertex.
        if (triarea2(portalApex, portalLeft, left) <= 0.0f)
        {
            if (vequal(portalApex, portalLeft) || triarea2(portalApex, portalRight, left) > 0.0f)
            {
                // Tighten the funnel.
                portalLeft = left;
                leftIndex = i;
            }
            else
            {
                // Left over right, insert right to path and restart scan from portal right point.
                output.push_back(*portalRight);

                // Make current right the new apex.
                portalApex = portalRight;
                apexIndex = rightIndex;
                // Reset portal
                portalLeft = portalApex;
                portalRight = portalApex;
                leftIndex = apexIndex;
                rightIndex = apexIndex;
                // Restart scan
                i = apexIndex;
                continue;
            }
        }
    }
    // Append last point to path.
    output.push_back(*portalsRight.back());
}



void commonVtx(Triangle* a, Triangle* b, Vertex** right, Vertex** left)
{
    Vertex *a0 = a->v[0], *a1 = a->v[1], *a2 = a->v[2];
    Vertex *b0 = b->v[0], *b1 = b->v[1], *b2 = b->v[2];
    if ((a0 == b0 && a1 == b2) || (a0 == b2 && a1 == b1) || (a0 == b1 && a1 == b0)) {
        *left = a0;
        *right = a1;
        return;
    }
    if ((a2 == b0 && a0 == b2) || (a2 == b2 && a0 == b1) || (a2 == b1 && a0 == b0)) {
        *left = a2;
        *right = a0;
        return;
    }
    if ((a1 == b0 && a2 == b2) || (a1 == b2 && a2 == b1) || (a1 == b1 && a2 == b0)) {
        *left = a1;
        *right = a2;
        return;
    }
    throw Exception("Unexpeteced common vtx");

}


void Mesh::makePath(vector<Triangle*>& tripath, Vec2 start, Vec2 end, vector<Vec2>& output)
{
    vector<Vec2*> leftPath, rightPath;
    leftPath.reserve(tripath.size() + 1);
    rightPath.reserve(tripath.size() + 1);

    leftPath.push_back(&start);
    rightPath.push_back(&start);
    for (int i = 0; i < tripath.size() - 1; ++i) {
        Vertex *right, *left;
        commonVtx(tripath[i], tripath[i + 1], &right, &left);
        leftPath.push_back(&left->p);
        rightPath.push_back(&right->p);
    }

    leftPath.push_back(&end);
    rightPath.push_back(&end);

    stringPull(rightPath, leftPath, output);

}
