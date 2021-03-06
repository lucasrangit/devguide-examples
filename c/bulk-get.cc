#include <libcouchbase/couchbase.h>
#include <libcouchbase/api3.h>
#include <vector>
#include <string>

struct Result {
    lcb_error_t rc;
    std::string key;
    std::string value;
    lcb_CAS cas;

    Result(const lcb_RESPBASE *rb) :
        rc(rb->rc),
        key(reinterpret_cast<const char*>(rb->key), rb->nkey),
        cas(rb->cas) {
    }
};

typedef std::vector<Result> ResultList;

static void op_callback(lcb_t, int cbtype, const lcb_RESPBASE *rb)
{
    ResultList *results = reinterpret_cast<ResultList*>(rb->cookie);
    Result res(rb);

    if (cbtype == LCB_CALLBACK_GET && rb->rc == LCB_SUCCESS) {
        const lcb_RESPGET *rg = reinterpret_cast<const lcb_RESPGET*>(rb);
        res.value.assign(reinterpret_cast<const char*>(rg->value), rg->nvalue);
    }
    results->push_back(res);
}

int main(int argc, char **argv)
{
    lcb_t instance;
    lcb_create_st crst;
    lcb_error_t rc;

    memset(&crst, 0, sizeof crst);
    crst.version = 3;
    crst.v.v3.connstr = "couchbase://10.0.0.31/default";
    rc = lcb_create(&instance, &crst);
    rc = lcb_connect(instance);
    lcb_wait(instance);
    rc = lcb_get_bootstrap_status(instance);

    lcb_install_callback3(instance, LCB_CALLBACK_GET, op_callback);

    // Make a list of keys to store initially
    std::vector<std::string> toGet;
    toGet.push_back("foo");
    toGet.push_back("bar");
    toGet.push_back("baz");

    ResultList results;

    lcb_sched_enter(instance);
    std::vector<std::string>::const_iterator its = toGet.begin();
    for (; its != toGet.end(); its++) {
        lcb_CMDGET gcmd = { 0 };
        LCB_CMD_SET_KEY(&gcmd, its->c_str(), its->size());
        rc = lcb_get3(instance, &results, &gcmd);
        if (rc != LCB_SUCCESS) {
            fprintf(stderr, "Couldn't schedule item %s: %s\n",
                its->c_str(), lcb_strerror(NULL, rc));

            // Unschedules all operations since the last scheduling context
            // (created by lcb_sched_enter)
            lcb_sched_fail(instance);
            break;
        }
    }
    lcb_sched_leave(instance);
    lcb_wait(instance);

    ResultList::iterator itr;
    for (itr = results.begin(); itr != results.end(); ++itr) {
        printf("%s: ", itr->key.c_str());
        if (itr->rc != LCB_SUCCESS) {
            printf("Failed (%s)\n", lcb_strerror(NULL, itr->rc));
        } else {
            printf("Value=%.*s. CAS=%llu\n",
                (int)itr->value.size(),itr->value.c_str(), itr->cas);
        }
    }

    lcb_destroy(instance);
    return 0;
}
