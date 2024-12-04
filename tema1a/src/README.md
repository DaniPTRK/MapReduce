# Tema 1 APD 2024 - Ion Daniel 335CC
## Modelul Map-Reduce

### Implementare
Pentru a obtine un index inversat organizat alfabetic folosind paradigma
Map-Reduce, am utilizat M+R thread-uri, M fiind numarul de thread-uri
asociate thread-urilor Mapper, iar R numarul de thread-uri Reducer.

Inainte de a crea thread-urile, sunt citite din fisierul primit ca argument
numele fisierelor text ce urmeaza sa fie prelucrate de catre Mappere, aceste
nume fiind salvate intr-o coada file_queue ce va fi folosita de catre
thread-urile Mapper.

La crearea thread-urilor, am realizat 2 functii distincte, o functie asociata
thread-urilor Mapper, numita intuitiv mapper_function(), si o functie asociata
thread-urilor Reducer, reducer_function().

Argumentul trimis catre functia mapper_function() este sub forma unui struct,
anume struct mapper_input. Continutul acestui struct este urmatorul:

```C++
// Input for map threads.
struct mapper_input {
    int thread_id;

    // Counter which tracks the current position
    // inside the queue of files.
    int *counter;

    // The queue of files.
    queue<string> *file_queue;

    // Vector of partial lists obtained.
    vector<unordered_map<string, set<int>>> *partial_list;

    // Sync data.
    pthread_mutex_t *mutex;
    pthread_barrier_t *barrier;
};
```

Astfel, avem:
* thread_id-ul reprezinta id-ul asociat thread-ului ce executa functia;
* counter-ul reprezinta un pointer catre un contor ce indica la al catelea
fisier din coada de fisiere ne aflam , intrucat suntem interesati sa punem
in listele partiale obtinute din urma procesului map ID-ul fisierului din care
am extras un cuvant;
* file_queue este coada de fisiere, obtinuta folosind functia extract_data();
* partial_list sunt listele partiale rezultate in urma executiilor thread-urilor mapper;
* mutex este un pointer catre un mutex ce este folosit atunci cand extragem o valoare
din coada si cand incrementam counter-ul ce este impartit intre thread-uri;
* barrier este un pointer catre o bariera ce este impartita intre thread-urile Mapper
si thread-urile Reducer, aceasta bariera este menita sa puna in asteptare thread-urile
Reducer pana cand thread-urile Mapper isi termina activitatea.

In mapper_function() thread-urile intra intr-un while() din care ies doar daca
file_queue-ul devine gol, adica atunci cand nu mai avem fisiere prin care mapperele sa
treaca. Daca un thread gaseste un fisier in file_queue, acesta il extrage, trece cuvant
cu cuvant prin fisierul respectiv si trece ceea ce gaseste, formatand cuvintele citite
astfel incat majusculele sa devina minuscule si cuvintele sa nu contina vreun char special.
Ceea ce se gaseste in fisier este inserat intr-un vector de unordered_map-uri, fiecare
pozitie din vector fiind asignata unui thread prin thread_id-ul sau. Intr-un
unordered_map, thread-urile insereaza cuvantul si ID-ul fisierului. Set-ul utilizat
ca valoare pentru unordered_map-uri este necesar in cazul in care un thread citeste mai multe
fisiere si gaseste, printre cuvinte, cuvinte care sunt deja in map, astfel inserand
in set ID-ul fisierul distinct din care s-a citit un cuvant peste care am mai dat in
trecut in alt fisier. De asemenea, am ales unordered_map pentru eficienta inserarii
elementelor si set pentru faptul ca sorteaza automat elementele. Dupa rularea tuturor
thread-urilor Mapper, partial_list-urile vor contine rezultatele thread-urilor,
impartite pe thread_id-ul fiecarui thread.

Argumentul trimis catre functia reducer_function() este sub forma unui struct,
anume struct reducer_input. Continutul acestui struct este urmatorul:

```C

// Input for reducer threads.
struct reducer_input {
    // Counter which indicates the next letter that will
    // be processed.
    char *counter;

    // Vector of partial lists received from mappers.
    vector<unordered_map<string, set<int>>> *partial_list;

    // Sync data.
    pthread_mutex_t *mutex;
    pthread_barrier_t *barrier;
};

```

Astfel, avem:
* counter-ul reprezinta un pointer catre un contor care indica ce litera
urmeaza sa fie cautata de catre Reducer in listele partiale realizate de
catre Mappere;
* partial_list ce reprezinta rezultatul rularii thread-urilor Mapper, adica
un vector ce contine perechi de cuvinte si set-uri de ID-uri de fisier in
care au fost gasite aceste cuvinte;
* mutex este un pointer catre un mutex ce este folosit atunci cand un thread
Reducer doreste sa caute o litera noua in listele partiale si sa creeze un
nou fisier pentru litera respectiva;
* barrier este un pointer catre bariera ce blocheaza thread-urile Reducer
din a porni pana cand thread-urile Mapper isi termina activititea.

In reducer_function(), thread-urile intra intr-un while() din care ies
atunci cand toate literele din alfabet, de la a la z, au un fisier asociat.
Acest fisier va contine toate cuvintele ce incep cu acea litera, precum
si ID-urile fisierelor din care am extras cuvintele respective. Dupa ce
un thread reducer extrage o litera, acesta trece prin toate listele
partiale, cautand cuvinte ce incep cu litera respectiva, iar atunci
cand gaseste un cuvant ce indeplineste aceasta conditie, il insereaza
intr-un unordered_map, aggr_map ce are ca si cheie cuvantul gasit,
iar ca valoare ID-urile fisierelor. Map-ul este util intrucat,
daca gasim de mai multe ori un cuvant in listele partiale, putem
accesa usor locatia unde se afla cuvantul in map si putem insera
ID-urile diferite in set-ul asociat cuvantului. Dupa aceasta, trecem
toate valorile din map intr-un vector ce-l sortam cu un comparator(),
functie ce ne permite sa sortam elementele vectorului descrescator dupa
numarul de fisier_ID asociate unui cuvant si, in caz de egalitate, alfabetic.
Dupa sortare, este creat un fisier cu numele contor_intern.txt,
unde contor_intern este litera careia ii cautam cuvintele, iar in acest
fisier sunt inserate toate valorile din vector, in ordinea obtinuta dupa
sortare. Dupa asocierea fiecarei litere cu un fisier .txt, thread-urile
Reducer isi incheie activitatea.

Astfel, pornind de la un numar M de thread-uri Mapper, un numar R de
thread-uri Reducer si un fisier ce continte alte fisiere, obtinem
fisiere pentru fiecare litera din alfabet, fisiere ce contin cuvinte
ce incep cu litera respectiva, cuvinte prezente in unul sau mai multe
fisiere primite ca input.