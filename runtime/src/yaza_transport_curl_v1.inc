/* Optional certificate-verified outbound transport. Included by yaza_runtime_v1.c. */
#if defined(YAZA_RT_V1_WITH_CURL)
static char *yaza_transport_copy_text_v1(const YazaTextV1 *value, size_t maximum) {
    if (value == NULL || value->length > maximum || (value->length > 0 && value->data == NULL)) return NULL;
    char *copy = (char *)malloc((size_t)value->length + 1);
    if (copy == NULL) return NULL;
    for (uint64_t i = 0; i < value->length; i++) {
        const unsigned char ch = value->data[i];
        if (ch == 0 || ch == '\r' || ch == '\n') {
            free(copy);
            return NULL;
        }
        copy[i] = (char)ch;
    }
    copy[value->length] = '\0';
    return copy;
}

static int yaza_transport_mailbox_valid_v1(const char *address) {
    if (address == NULL) return 0;
    const size_t length = strlen(address);
    if (length < 5 || length > 254) return 0;
    size_t at = SIZE_MAX;
    int domain_dot = 0;
    for (size_t i = 0; i < length; i++) {
        const unsigned char ch = (unsigned char)address[i];
        if (ch == '@') {
            if (at != SIZE_MAX) return 0;
            at = i;
            continue;
        }
        if (ch <= 32 || ch >= 127 || strchr("<>\"(),:;[\\]", ch) != NULL) return 0;
        if (ch == '.' && (i == 0 || i + 1 == length || address[i - 1] == '.')) return 0;
        if (at != SIZE_MAX) {
            const int domain_char = (ch >= 'A' && ch <= 'Z') ||
                (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') ||
                ch == '-' || ch == '.';
            if (!domain_char) return 0;
            if (ch == '.') {
                domain_dot = 1;
                if (address[i - 1] == '-' || address[i - 1] == '@') return 0;
            }
            if (ch == '-' && (address[i - 1] == '.' || address[i - 1] == '@')) return 0;
        }
    }
    if (at == SIZE_MAX || at < 1 || at > 64 || at + 3 >= length || !domain_dot) return 0;
    if (address[at - 1] == '.' || address[at + 1] == '.' || address[length - 1] == '-') return 0;
    return 1;
}

static pthread_once_t yaza_transport_curl_once_v1 = PTHREAD_ONCE_INIT;
static int yaza_transport_curl_ready_v1 = 0;

static void yaza_transport_curl_init_v1(void) {
    yaza_transport_curl_ready_v1 = curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
}

/* Trust a deployment-specific CA without weakening peer or host verification. */
static const char *yaza_transport_ca_bundle_v1(void) {
    const char *path = getenv("YAZA_TLS_CA_BUNDLE");
    if (path == NULL || path[0] == '\0') return NULL;
    return path;
}

typedef struct YazaTransportWriteV1 {
    unsigned char *data;
    size_t length;
    size_t maximum;
    int exceeded;
} YazaTransportWriteV1;

static size_t yaza_transport_write_v1(char *input, size_t size, size_t count, void *opaque) {
    YazaTransportWriteV1 *output = (YazaTransportWriteV1 *)opaque;
    if (size != 0 && count > SIZE_MAX / size) return 0;
    const size_t length = size * count;
    if (length > output->maximum - output->length) {
        output->exceeded = 1;
        return 0;
    }
    memcpy(output->data + output->length, input, length);
    output->length += length;
    return length;
}

typedef struct YazaTransportReadV1 {
    const unsigned char *data;
    size_t length;
    size_t cursor;
} YazaTransportReadV1;

static size_t yaza_transport_read_v1(char *output, size_t size, size_t count, void *opaque) {
    YazaTransportReadV1 *input = (YazaTransportReadV1 *)opaque;
    if (size != 0 && count > SIZE_MAX / size) return CURL_READFUNC_ABORT;
    const size_t capacity = size * count;
    size_t left = input->length - input->cursor;
    if (left > capacity) left = capacity;
    if (left > 0) memcpy(output, input->data + input->cursor, left);
    input->cursor += left;
    return left;
}
#endif

int32_t yaza_net_v1_https_request(const YazaTextV1 *method, const YazaTextV1 *url,
    const YazaBytesV1 *body, const YazaTextV1 *bearer_token,
    int64_t timeout_ms, int64_t max_response_bytes, YazaBytesV1 *out_wire, char **out_error) {
    if (out_error == NULL || out_wire == NULL) return 0;
    *out_error = NULL;
    out_wire->data = NULL;
    out_wire->length = 0;
    if (timeout_ms < 1 || timeout_ms > 60000 || max_response_bytes < 1 ||
        max_response_bytes > 16 * 1024 * 1024 || body == NULL ||
        (body->length > 0 && body->data == NULL) || body->length > 16 * 1024 * 1024) {
        *out_error = (char *)"HTTPS request limits or body are invalid";
        return 0;
    }
#if !defined(YAZA_RT_V1_WITH_CURL)
    (void)method; (void)url; (void)bearer_token;
    *out_error = (char *)"HTTPS client is unavailable in this build (libcurl required)";
    return 0;
#else
    char *method_c = yaza_transport_copy_text_v1(method, 8);
    char *url_c = yaza_transport_copy_text_v1(url, 2048);
    char *bearer_c = yaza_transport_copy_text_v1(bearer_token, 4096);
    if (method_c == NULL || url_c == NULL || bearer_c == NULL ||
        strncmp(url_c, "https://", 8) != 0 ||
        !(strcmp(method_c, "GET") == 0 || strcmp(method_c, "HEAD") == 0 ||
          strcmp(method_c, "POST") == 0 || strcmp(method_c, "PUT") == 0 ||
          strcmp(method_c, "PATCH") == 0 || strcmp(method_c, "DELETE") == 0)) {
        free(method_c); free(url_c); free(bearer_c);
        *out_error = (char *)"HTTPS method, URL, or bearer token is invalid";
        return 0;
    }
    if ((strcmp(method_c, "GET") == 0 || strcmp(method_c, "HEAD") == 0) && body->length > 0) {
        free(method_c); free(url_c); free(bearer_c);
        *out_error = (char *)"GET and HEAD requests cannot carry a body";
        return 0;
    }
    (void)pthread_once(&yaza_transport_curl_once_v1, yaza_transport_curl_init_v1);
    if (!yaza_transport_curl_ready_v1) {
        free(method_c); free(url_c); free(bearer_c);
        *out_error = (char *)"HTTPS transport initialization failed";
        return 0;
    }
    CURL *curl = curl_easy_init();
    unsigned char *response = (unsigned char *)malloc((size_t)max_response_bytes);
    struct curl_slist *headers = NULL;
    if (curl == NULL || response == NULL) {
        if (curl != NULL) curl_easy_cleanup(curl);
        free(response); free(method_c); free(url_c); free(bearer_c);
        *out_error = (char *)"HTTPS transport allocation failed";
        return 0;
    }
    YazaTransportWriteV1 output = {response, 0, (size_t)max_response_bytes, 0};
    CURLcode setup = CURLE_OK;
#define YAZA_CURL_SET(option, value) do { if (setup == CURLE_OK) setup = curl_easy_setopt(curl, option, value); } while (0)
    YAZA_CURL_SET(CURLOPT_URL, url_c);
    YAZA_CURL_SET(CURLOPT_PROTOCOLS_STR, "https");
    YAZA_CURL_SET(CURLOPT_FOLLOWLOCATION, 0L);
    YAZA_CURL_SET(CURLOPT_SSL_VERIFYPEER, 1L);
    YAZA_CURL_SET(CURLOPT_SSL_VERIFYHOST, 2L);
    const char *ca_bundle = yaza_transport_ca_bundle_v1();
    if (ca_bundle != NULL) YAZA_CURL_SET(CURLOPT_CAINFO, ca_bundle);
    YAZA_CURL_SET(CURLOPT_NOSIGNAL, 1L);
    YAZA_CURL_SET(CURLOPT_TIMEOUT_MS, (long)timeout_ms);
    YAZA_CURL_SET(CURLOPT_CONNECTTIMEOUT_MS, (long)timeout_ms);
    YAZA_CURL_SET(CURLOPT_MAXFILESIZE_LARGE, (curl_off_t)max_response_bytes);
    YAZA_CURL_SET(CURLOPT_WRITEFUNCTION, yaza_transport_write_v1);
    YAZA_CURL_SET(CURLOPT_WRITEDATA, &output);
    YAZA_CURL_SET(CURLOPT_CUSTOMREQUEST, method_c);
    if (strcmp(method_c, "HEAD") == 0) YAZA_CURL_SET(CURLOPT_NOBODY, 1L);
    if (body->length > 0 || strcmp(method_c, "POST") == 0 || strcmp(method_c, "PUT") == 0 ||
        strcmp(method_c, "PATCH") == 0) {
        YAZA_CURL_SET(CURLOPT_POSTFIELDS, body->length > 0 ? (const char *)body->data : "");
        YAZA_CURL_SET(CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)body->length);
        headers = curl_slist_append(headers, "Content-Type: application/json");
    }
    if (bearer_c[0] != '\0') {
        const size_t length = strlen(bearer_c);
        char *authorization = (char *)malloc(length + 23);
        if (authorization == NULL) setup = CURLE_OUT_OF_MEMORY;
        else {
            memcpy(authorization, "Authorization: Bearer ", 22);
            memcpy(authorization + 22, bearer_c, length + 1);
            struct curl_slist *next = curl_slist_append(headers, authorization);
            if (next == NULL) setup = CURLE_OUT_OF_MEMORY;
            else headers = next;
            free(authorization);
        }
    }
    if (headers != NULL) YAZA_CURL_SET(CURLOPT_HTTPHEADER, headers);
    CURLcode result = setup == CURLE_OK ? curl_easy_perform(curl) : setup;
    long status = 0;
    if (result == CURLE_OK) result = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    if (result == CURLE_OK && (status < 100 || status > 599)) result = CURLE_HTTP_RETURNED_ERROR;
    if (result == CURLE_OK) {
        unsigned char *wire = (unsigned char *)yaza_rt_v1_alloc(output.length + 2);
        if (wire == NULL) result = CURLE_OUT_OF_MEMORY;
        else {
            wire[0] = (unsigned char)((status >> 8) & 0xff);
            wire[1] = (unsigned char)(status & 0xff);
            if (output.length > 0) memcpy(wire + 2, response, output.length);
            out_wire->data = wire;
            out_wire->length = output.length + 2;
        }
    }
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(response); free(method_c); free(url_c); free(bearer_c);
    if (result != CURLE_OK) {
        *out_error = output.exceeded ? (char *)"HTTPS response exceeds configured byte limit"
            : (char *)curl_easy_strerror(result);
        return 0;
    }
    return 1;
#undef YAZA_CURL_SET
#endif
}

int32_t yaza_net_v1_smtp_secure_send(const YazaTextV1 *provider, const YazaTextV1 *auth_mode,
    const YazaTextV1 *username, const YazaTextV1 *credential,
    const YazaTextV1 *from_address, const YazaTextV1 *to_address,
    const YazaTextV1 *message, int64_t timeout_ms, char **out_error) {
    if (out_error == NULL) return 0;
    *out_error = NULL;
    if (timeout_ms < 1 || timeout_ms > 60000 || message == NULL ||
        message->data == NULL || message->length == 0 || message->length > 1024 * 1024) {
        *out_error = (char *)"SMTP message or timeout is invalid";
        return 0;
    }
#if !defined(YAZA_RT_V1_WITH_CURL)
    (void)provider; (void)auth_mode; (void)username; (void)credential;
    (void)from_address; (void)to_address;
    *out_error = (char *)"secure SMTP is unavailable in this build (libcurl required)";
    return 0;
#else
    char *provider_c = yaza_transport_copy_text_v1(provider, 16);
    char *auth_c = yaza_transport_copy_text_v1(auth_mode, 16);
    char *username_c = yaza_transport_copy_text_v1(username, 254);
    char *credential_c = yaza_transport_copy_text_v1(credential, 4096);
    char *from_c = yaza_transport_copy_text_v1(from_address, 254);
    char *to_c = yaza_transport_copy_text_v1(to_address, 254);
    if (provider_c == NULL || auth_c == NULL || username_c == NULL || credential_c == NULL ||
        from_c == NULL || to_c == NULL || username_c[0] == '\0' || credential_c[0] == '\0' ||
        !yaza_transport_mailbox_valid_v1(from_c) ||
        !yaza_transport_mailbox_valid_v1(to_c) ||
        (strcmp(auth_c, "password") != 0 && strcmp(auth_c, "oauth2") != 0)) {
        free(provider_c); free(auth_c); free(username_c); free(credential_c); free(from_c); free(to_c);
        *out_error = (char *)"SMTP provider, authentication, or address is invalid";
        return 0;
    }
    const char *url = NULL;
    if (strcmp(provider_c, "google") == 0) url = "smtp://smtp.gmail.com:587";
    if (strcmp(provider_c, "brevo") == 0 && strcmp(auth_c, "password") == 0)
        url = "smtp://smtp-relay.brevo.com:587";
    if (url == NULL) {
        free(provider_c); free(auth_c); free(username_c); free(credential_c); free(from_c); free(to_c);
        *out_error = (char *)"SMTP provider or authentication mode is unsupported";
        return 0;
    }
    (void)pthread_once(&yaza_transport_curl_once_v1, yaza_transport_curl_init_v1);
    if (!yaza_transport_curl_ready_v1) {
        free(provider_c); free(auth_c); free(username_c); free(credential_c); free(from_c); free(to_c);
        *out_error = (char *)"SMTP transport initialization failed";
        return 0;
    }
    CURL *curl = curl_easy_init();
    char *sender = (char *)malloc(strlen(from_c) + 3);
    char *recipient = (char *)malloc(strlen(to_c) + 3);
    struct curl_slist *recipients = NULL;
    if (curl == NULL || sender == NULL || recipient == NULL) {
        if (curl != NULL) curl_easy_cleanup(curl);
        free(sender); free(recipient);
        free(provider_c); free(auth_c); free(username_c); free(credential_c); free(from_c); free(to_c);
        *out_error = (char *)"SMTP transport allocation failed";
        return 0;
    }
    snprintf(sender, strlen(from_c) + 3, "<%s>", from_c);
    snprintf(recipient, strlen(to_c) + 3, "<%s>", to_c);
    recipients = curl_slist_append(NULL, recipient);
    if (recipients == NULL) {
        curl_easy_cleanup(curl);
        free(sender); free(recipient);
        free(provider_c); free(auth_c); free(username_c); free(credential_c); free(from_c); free(to_c);
        *out_error = (char *)"SMTP recipient allocation failed";
        return 0;
    }
    YazaTransportReadV1 input = {message->data, (size_t)message->length, 0};
    CURLcode setup = CURLE_OK;
#define YAZA_SMTP_SET(option, value) do { if (setup == CURLE_OK) setup = curl_easy_setopt(curl, option, value); } while (0)
    YAZA_SMTP_SET(CURLOPT_URL, url);
    YAZA_SMTP_SET(CURLOPT_PROTOCOLS_STR, "smtp");
    YAZA_SMTP_SET(CURLOPT_USE_SSL, CURLUSESSL_ALL);
    YAZA_SMTP_SET(CURLOPT_SSL_VERIFYPEER, 1L);
    YAZA_SMTP_SET(CURLOPT_SSL_VERIFYHOST, 2L);
    const char *ca_bundle = yaza_transport_ca_bundle_v1();
    if (ca_bundle != NULL) YAZA_SMTP_SET(CURLOPT_CAINFO, ca_bundle);
    YAZA_SMTP_SET(CURLOPT_NOSIGNAL, 1L);
    YAZA_SMTP_SET(CURLOPT_TIMEOUT_MS, (long)timeout_ms);
    YAZA_SMTP_SET(CURLOPT_CONNECTTIMEOUT_MS, (long)timeout_ms);
    YAZA_SMTP_SET(CURLOPT_USERNAME, username_c);
    if (strcmp(auth_c, "oauth2") == 0) {
        YAZA_SMTP_SET(CURLOPT_XOAUTH2_BEARER, credential_c);
        YAZA_SMTP_SET(CURLOPT_LOGIN_OPTIONS, "AUTH=XOAUTH2");
    } else {
        YAZA_SMTP_SET(CURLOPT_PASSWORD, credential_c);
    }
    YAZA_SMTP_SET(CURLOPT_MAIL_FROM, sender);
    YAZA_SMTP_SET(CURLOPT_MAIL_RCPT, recipients);
    YAZA_SMTP_SET(CURLOPT_READFUNCTION, yaza_transport_read_v1);
    YAZA_SMTP_SET(CURLOPT_READDATA, &input);
    YAZA_SMTP_SET(CURLOPT_INFILESIZE_LARGE, (curl_off_t)message->length);
    YAZA_SMTP_SET(CURLOPT_UPLOAD, 1L);
    CURLcode result = setup == CURLE_OK ? curl_easy_perform(curl) : setup;
    curl_slist_free_all(recipients);
    curl_easy_cleanup(curl);
    free(sender); free(recipient);
    free(provider_c); free(auth_c); free(username_c);
    if (credential_c != NULL) {
        memset(credential_c, 0, strlen(credential_c));
        free(credential_c);
    }
    free(from_c); free(to_c);
    if (result != CURLE_OK) {
        *out_error = (char *)curl_easy_strerror(result);
        return 0;
    }
    return 1;
#undef YAZA_SMTP_SET
#endif
}
