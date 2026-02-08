#include <jvmti.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#define MAX_SYMBOLS 10000

typedef struct
{
    const void *code_start;
    const void *code_end;
    char *method_name;
    char *class_sig;
    char *method_sig;
    jmethodID method;
} Symbol;

static Symbol symbol_table[MAX_SYMBOLS];
static int symbol_count = 0;
static pthread_mutex_t symbol_lock = PTHREAD_MUTEX_INITIALIZER;

static jvmtiEnv *jvmti = NULL;

const char *lookup_symbol(const void *addr)
{
    pthread_mutex_lock(&symbol_lock);
    for (int i = 0; i < symbol_count; i++)
    {
        if (addr >= symbol_table[i].code_start &&
            addr < symbol_table[i].code_end)
        {
            const char *name = symbol_table[i].method_name;
            pthread_mutex_unlock(&symbol_lock);
            return name;
        }
    }
    pthread_mutex_unlock(&symbol_lock);
    return "UNKNOWN";
}

void JNICALL compiledMethodLoadHandler(jvmtiEnv *jvmti,
                                           jmethodID method,
                                           jint code_size,
                                           const void *code_addr,
                                           jint map_length,
                                           const jvmtiAddrLocationMap *map,
                                           const void *compile_info)
{
    char *method_name = NULL;
    char *method_sig = NULL;
    char *class_sig = NULL;
    jclass clazz;

    (*jvmti)->GetMethodDeclaringClass(jvmti, method, &clazz);
    (*jvmti)->GetMethodName(jvmti, method, &method_name, &method_sig, NULL);
    (*jvmti)->GetClassSignature(jvmti, clazz, &class_sig, NULL);

    pthread_mutex_lock(&symbol_lock);

    if (symbol_count < MAX_SYMBOLS)
    {
        symbol_table[symbol_count].code_start = code_addr;
        symbol_table[symbol_count].code_end =
            (void *)((char *)code_addr + code_size);

        symbol_table[symbol_count].method_name = strdup(method_name);
        symbol_table[symbol_count].method_sig = strdup(method_sig);
        symbol_table[symbol_count].class_sig = strdup(class_sig);
        symbol_table[symbol_count].method = method;

        // printf("[agent] #%d  %s::%s  [%p – %p]  (%d bytes)\n",
            //    symbol_count, class_sig, method_name,
            //    code_addr, (char *)code_addr + code_size, code_size);

        symbol_count++;
    }

    pthread_mutex_unlock(&symbol_lock);

    (*jvmti)->Deallocate(jvmti, (unsigned char *)method_name);
    (*jvmti)->Deallocate(jvmti, (unsigned char *)method_sig);
    (*jvmti)->Deallocate(jvmti, (unsigned char *)class_sig);
}

void JNICALL compiledMethodUnloadHandler(jvmtiEnv *jvmti, jmethodID method, const void *code_addr)
{
    pthread_mutex_lock(&symbol_lock);
    for (int i = 0; i < symbol_count; i++)
    {
        if (symbol_table[i].method == method)
        {
            // printf("[agent] Unloaded: %s\n", symbol_table[i].method_name);
            free(symbol_table[i].method_name);
            free(symbol_table[i].method_sig);
            free(symbol_table[i].class_sig);
            for (int j = i; j < symbol_count - 1; j++)
            {
                symbol_table[j] = symbol_table[j + 1];
            }
            symbol_count--;
            break;
        }
    }
    pthread_mutex_unlock(&symbol_lock);
}

void JNICALL vmInitHandler(jvmtiEnv *jvmti, JNIEnv *jni, jthread thread)
{
    // printf("[agent] VM initialized — requesting retroactive CompiledMethodLoad events\n");
    jvmtiError err = (*jvmti)->GenerateEvents(jvmti, JVMTI_EVENT_COMPILED_METHOD_LOAD);
    if (err != JVMTI_ERROR_NONE);
}

void *input_thread(void *arg)
{
    while (1)
    {
        unsigned long long addr;
        printf("Enter address (hex): ");
        fflush(stdout);
        if (scanf("%llx", &addr) != 1)
        {
            break;
        }
        const char *symbol = lookup_symbol((const void *)addr);
        printf("Symbol for 0x%llx: %s  (symbol_count=%d)\n", addr, symbol, symbol_count);
    }
    return NULL;
}

JNIEXPORT jint JNICALL
Agent_OnLoad(JavaVM *vm, char *options, void *reserved)
{

    jint res = (*vm)->GetEnv(vm, (void **)&jvmti, JVMTI_VERSION_1_2);
    if (res != JNI_OK)
        return JNI_ERR;

    jvmtiCapabilities caps = {0};
    caps.can_generate_compiled_method_load_events = 1;
    (*jvmti)->AddCapabilities(jvmti, &caps);

    jvmtiEventCallbacks callbacks = {0};
    callbacks.CompiledMethodLoad = &compiledMethodLoadHandler;
    callbacks.CompiledMethodUnload = &compiledMethodUnloadHandler;
    callbacks.VMInit = &vmInitHandler;
    (*jvmti)->SetEventCallbacks(jvmti, &callbacks, sizeof(callbacks));

    (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE,
                                       JVMTI_EVENT_COMPILED_METHOD_LOAD, NULL);
    (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE,
                                       JVMTI_EVENT_COMPILED_METHOD_UNLOAD, NULL);
    (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE,
                                       JVMTI_EVENT_VM_INIT, NULL);

    // printf("[agent] Loaded — waiting for VM init to generate retroactive events\n");

    pthread_t thread;
    pthread_create(&thread, NULL, input_thread, NULL);
    pthread_detach(thread);

    return JNI_OK;
}
