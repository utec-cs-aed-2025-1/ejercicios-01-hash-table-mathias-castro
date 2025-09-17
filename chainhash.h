#include <vector>
#include <functional>
#include <stdexcept>
#include <cstddef>
using namespace std;

const int   maxColision   = 3;
const float maxFillFactor = 0.8f;

template<typename TK, typename TV>
struct ChainHashNode {
    TK key;
    TV value;
    ChainHashNode* next;
    ChainHashNode(TK key, TV value) : key(key), value(value), next(nullptr) {}
};

template<typename TK, typename TV>
class ChainHashListIterator {
    using Node = ChainHashNode<TK, TV>;
    Node* current;

public:
    using iterator_category = std::forward_iterator_tag;
    using value_type        = Node;
    using difference_type   = std::ptrdiff_t;
    using pointer           = Node*;
    using reference         = Node&;

    explicit ChainHashListIterator(Node* node = nullptr) : current(node) {}

    reference operator*()  const { return *current; }
    pointer   operator->() const { return  current; }

    ChainHashListIterator& operator++() {          // ++it
        if (current) current = current->next;
        return *this;
    }
    ChainHashListIterator operator++(int) {        // it++
        ChainHashListIterator tmp = *this;
        ++(*this);
        return tmp;
    }
    bool operator==(const ChainHashListIterator& other) const { return current == other.current; }
    bool operator!=(const ChainHashListIterator& other) const { return !(*this == other); }
};

template<typename TK, typename TV>
class ChainHash {
private:
    typedef ChainHashNode<TK, TV>             Node;
    typedef ChainHashListIterator<TK, TV>     Iterator;

    Node** array;          // arreglo de punteros a Node (buckets)
    int nsize;             // # de pares <key:value>
    int capacity;          // tamaño del arreglo
    int* bucket_sizes;     // # de elementos por bucket
    int usedBuckets;       // # de buckets no vacíos

public:
    ChainHash(int initialCapacity = 10){
        if (initialCapacity <= 0) initialCapacity = 10;
        this->capacity      = initialCapacity;
        this->array         = new Node*[capacity]();
        this->bucket_sizes  = new int[capacity]();
        this->nsize         = 0;
        this->usedBuckets   = 0;
    }

	ChainHash(ChainHash&& other) noexcept
        : array(other.array), nsize(other.nsize), capacity(other.capacity),
          bucket_sizes(other.bucket_sizes), usedBuckets(other.usedBuckets) {
        other.array = nullptr; other.bucket_sizes = nullptr;
        other.capacity = 0; other.nsize = 0; other.usedBuckets = 0;
    }

    ChainHash& operator=(ChainHash&& other) noexcept {
        if (this != &other) {
            // libera lo actual
            if (array){
                for (int i = 0; i < capacity; ++i){
                    Node* cur = array[i];
                    while (cur){ Node* nxt = cur->next; delete cur; cur = nxt; }
                }
                delete[] array;
            }
            delete[] bucket_sizes;

            // toma propiedad
            array = other.array; bucket_sizes = other.bucket_sizes;
            capacity = other.capacity; nsize = other.nsize; usedBuckets = other.usedBuckets;

            // deja other nulo
            other.array = nullptr; other.bucket_sizes = nullptr;
            other.capacity = 0; other.nsize = 0; other.usedBuckets = 0;
        }
        return *this;
    }

    // Evitamos copias superficiales peligrosas (opcional)
    ChainHash(const ChainHash&)            = delete;
    ChainHash& operator=(const ChainHash&) = delete;

    TV get(TK key){
        size_t hashcode = getHashCode(key);
        size_t index    = hashcode % capacity;

        Node* current = this->array[index];
        while (current != nullptr){
            if (current->key == key) return current->value;
            current = current->next;
        }
        throw std::out_of_range("Key no encontrado");
    }

    int size() const { return this->nsize; }
    int bucket_count() const { return this->capacity; }

    int bucket_size(int index) {
        if (index < 0 || index >= this->capacity) throw std::out_of_range("Indice de bucket invalido");
        return this->bucket_sizes[index];
    }

    // Inserta/actualiza
    void set(TK key, TV value){
        size_t idx = getHashCode(key) % capacity;
        Node*  cur = array[idx];

        // Actualización si ya existe
        while (cur){
            if (cur->key == key){
                cur->value = value;
                return;
            }
            cur = cur->next;
        }

        // Inserción al frente del bucket
        Node* nuevo = new Node(key, value);
        nuevo->next = array[idx];
        array[idx]  = nuevo;

        if (bucket_sizes[idx] == 0) usedBuckets++;
        bucket_sizes[idx]++;
        nsize++;

        // Rehash por colisiones o por fill factor
        if (bucket_sizes[idx] > maxColision || fillFactor() > maxFillFactor){
            rehashing();
        }
    }

    // Elimina por clave
    bool remove(TK key){
        size_t idx = getHashCode(key) % capacity;
        Node*  cur = array[idx];
        Node*  prev = nullptr;

        while (cur){
            if (cur->key == key){
                if (prev) prev->next = cur->next;
                else      array[idx] = cur->next;

                delete cur;
                bucket_sizes[idx]--;
                nsize--;

                if (bucket_sizes[idx] == 0) usedBuckets--;
                return true;
            }
            prev = cur;
            cur  = cur->next;
        }
        return false;
    }

    // Existe la clave
    bool contains(TK key){
        size_t idx = getHashCode(key) % capacity;
        Node*  cur = array[idx];
        while (cur){
            if (cur->key == key) return true;
            cur = cur->next;
        }
        return false;
    }

    // Iteradores para recorrer UN bucket (lista encadenada) específico
    Iterator begin(int index){
        if (index < 0 || index >= capacity) throw std::out_of_range("Indice de bucket invalido");
        return Iterator(array[index]);
    }
    Iterator end(int index){
        (void)index; // no se usa realmente
        return Iterator(nullptr);
    }

private:
    double fillFactor() const {
        return static_cast<double>(this->usedBuckets) / static_cast<double>(this->capacity);
    }

    size_t getHashCode(const TK& key){
        std::hash<TK> hasher;
        return hasher(key);
    }

    // Inserta un nodo YA CREADO en el nuevo arreglo (sin checks/rehash)
    void insertNodeNoCheck(Node* node, Node** newArray, int* newBucketSizes, int newCapacity, int& newUsedBuckets){
        size_t idx = getHashCode(node->key) % newCapacity;
        if (newBucketSizes[idx] == 0) newUsedBuckets++;
        node->next          = newArray[idx];
        newArray[idx]       = node;
        newBucketSizes[idx] += 1;
    }

    // Rehash: duplica capacidad y reubica todos los nodos
    void rehashing(){
        int   oldCapacity   = capacity;
        Node** oldArray     = array;
        int*   oldBucketSz  = bucket_sizes;

        int newCapacity = capacity * 2;
        if (newCapacity <= 0) newCapacity = capacity + 1; // fallback

        Node** newArray       = new Node*[newCapacity]();
        int*   newBucketSizes = new int[newCapacity]();
        int    newUsedBuckets = 0;

        // Reinsertar todos los nodos en la nueva tabla
        for (int i = 0; i < oldCapacity; ++i){
            Node* cur = oldArray[i];
            while (cur){
                Node* nxt = cur->next;
                cur->next = nullptr;
                insertNodeNoCheck(cur, newArray, newBucketSizes, newCapacity, newUsedBuckets);
                cur = nxt;
            }
        }

        // Liberar estructuras viejas (ya movimos nodos)
        delete[] oldArray;
        delete[] oldBucketSz;

        // Actualizar metadatos
        array        = newArray;
        bucket_sizes = newBucketSizes;
        capacity     = newCapacity;
        usedBuckets  = newUsedBuckets;
        // nsize se mantiene igual (mismo # de elementos)
    }

public:
    ~ChainHash(){
        if (array){
            for (int i = 0; i < capacity; ++i){
                Node* cur = array[i];
                while (cur){
                    Node* nxt = cur->next;
                    delete cur;
                    cur = nxt;
                }
            }
            delete[] array;
            array = nullptr;
        }
        if (bucket_sizes){
            delete[] bucket_sizes;
            bucket_sizes = nullptr;
        }
        nsize = 0;
        capacity = 0;
        usedBuckets = 0;
    }
};
