#include <iostream>
#include <iomanip>
#include <array>
#include <memory>
#include <thread>
#include <list>
#include <cstdlib>
#include <cmath>
#include <chrono>


#include <unistd.h>

const unsigned aLen = 1000000;
using namespace std;

struct Cell
{
    int a;
    int b;
    int c;
    Cell(int a=0, int b=0, int c=0) :
        a(a), b(b), c(c) {}
};

typedef array<Cell, aLen> ACell;

struct Field
{
    ACell arr;
    int sum=0;
    int index=0;
};

typedef shared_ptr<Field> PField;
struct FieldProc
{
    const PField last;
    PField curr;

    FieldProc(const PField state) : last(state)
    {
        curr.reset(new Field(*(last)));
    }
};
typedef unique_ptr<FieldProc> PProc;

PProc startProc(PField pField)
{
    PProc fp(new FieldProc(pField));
    fp->curr->index++;

    return fp;
}

PField endProc(PProc fp)
{
    PField answ(move(fp->curr));
    fp.reset();

    return answ;
}

PField randInit()
{
    srand(time(0));
    const int C = rand() % 32;
    PField pf(new Field);

    for (unsigned i = 0; i < aLen; i++)
    {
        pf->arr[i].a = rand() % 100;
        pf->arr[i].b = rand() % 64;
        pf->arr[i].c = C;

    }

    return pf;
}

typedef pair<int, int> chankRng;

struct ThreadInfo
{
    int indexThread;
    int countThreads;
    ThreadInfo(int idx, int count) :
        indexThread(idx), countThreads(count) {}
};

chankRng getRngOfThread(const ThreadInfo ti, const int len)
{
    const int chankLen = len / ti.countThreads;
    const int chanksPlusOneCount = len % ti.countThreads;

    const int startRng = (chankLen*ti.indexThread) +
            (chanksPlusOneCount % (ti.indexThread+1));
    const int endRng = startRng + chankLen + int(chanksPlusOneCount > ti.indexThread);

    return make_pair(startRng, endRng);
}

list<int> getNeighbors(const int index)
{
    list<int> ln;
    if (index > 0)
        ln.push_back(index-1);
    if (index+1 < (int)aLen)
        ln.push_back(index+1);

    return ln;
}

class IEngine
{
public:
    virtual PProc step(PProc proc, int countTreads) = 0;
    virtual string name()const = 0;
};
typedef unique_ptr<IEngine> PIEngine;

void calcA(const chankRng rng, const Field& in, Field& out)
{
    const auto& inArr = in.arr;

    for (int i = rng.first; i < rng.second; i++)
    {
        auto &outA = out.arr[i].a;
        auto nbrs = getNeighbors(i);
        int sum(0);
        for (const auto& n : nbrs)
            sum += inArr[n].a;
        const auto size = nbrs.size();
        outA = sum / size;
    }
}
void calcB(const chankRng rng, const Field& in, Field& out)
{
    const auto& inArr = in.arr;

    for (int i = rng.first; i < rng.second; i++)
    {
        auto &outB = out.arr[i].b;
        outB = inArr[(i+1) % aLen].b;
    }
}

using pFunCalc = void (*)(const chankRng rng, const Field& in, Field& out);
typedef std::list<pFunCalc> lstFunCalcs;

void threadCalc(const lstFunCalcs& tasks, const ThreadInfo ti, const Field& in, Field& out)
{
    const auto rng = getRngOfThread(ti, in.arr.size());
    for (const auto task : tasks)
        task(rng, in, out);
}

class ChankEngine : public IEngine
{
public:
    ChankEngine(lstFunCalcs& tasks) :
        m_pfCalcs(tasks)
    {}
    PProc step(PProc proc, int countTreads) override
    {
        ThreadInfo ti(0, countTreads);
        for (int i = 0; i < countTreads; i++)
        {
            ti.indexThread = i;
            m_threads.push_back(
                        std::thread(
                            threadCalc,
                            std::cref(m_pfCalcs),
                            ti,
                            std::cref(*(proc->last)),
                            std::ref(*(proc->curr)))
                        );
        }

        for (auto& thr : m_threads)
            thr.join();

        m_threads.clear();
        return proc;
    }
    string name()const override
    {
        return "Engine Chank";
    }

private:
    std::list<std::thread> m_threads;
    lstFunCalcs m_pfCalcs;
};

class MainEngine
{
public:
    MainEngine(int nCores=1) :
        m_nCores(nCores)
    {
        m_pState = randInit();
        lstFunCalcs tasks = {calcA, calcB};
        m_engs.push_back(PIEngine(new ChankEngine(tasks)));
    }


    const PField step()
    {
        PProc proc = startProc(m_pState);

        for (auto& eng : m_engs)
            proc = eng->step(move(proc), m_nCores);

        m_pState = endProc(move(proc));
        return m_pState;
    }

private:
    int m_nCores;
    PField m_pState;
    list<PIEngine> m_engs;
};

void calcByCountThread(const int nThr, const int N)
{
    MainEngine meng(nThr);
    PField curState;

    auto t1 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < N; i++)
        curState = meng.step();

    auto t2 = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>( t2 - t1 ).count();

    std::cout << duration / (10*N) << endl;
}

int main()
{

    int maxThreads = thread::hardware_concurrency();
    for (int i = 1; i <= maxThreads; i++)
    {
        cout << setw(3) << i << ": ";
        calcByCountThread(i, 100);
    }
    return 0;
}
