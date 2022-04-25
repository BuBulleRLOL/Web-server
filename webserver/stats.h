int init_stats(void);

typedef struct {
    int served_connections;
    int served_requests;
    int ok_200;
    int ko_400;
    int ko_403;
    int ko_404;
} web_stats;

web_stats *get_stats(void);
