static inline void __attribute__((always_inline))
poll_busy_add_n_pkts(CPUBusyInfo *cpu, uint32_t n_pkts)
{
    cpu->tls_cpu_n_pkts += n_pkts;
}

static inline void __attribute__((always_inline))
poll_busy_gather(CPUBusyInfo *cpu)
{
    uint64_t current_tsc = rte_rdtsc();
    static __thread uint64_t last_tsc = 0;

    if (cpu->tls_cpu_n_pkts) {
        cpu->pkt_total += cpu->tls_cpu_n_pkts;
        cpu->process_times++;
        cpu->process_tsc_total += current_tsc - last_tsc;

        cpu->tls_cpu_n_pkts = 0;
    }

    last_tsc = current_tsc;
}

static void __pollload_gather_cb(CPUWorker *worker)
{
    CPUBusyInfo *cpu = &worker->busy_info;
    const uint32_t pkt_total            = cpu->pkt_total;
    const uint32_t process_times        = cpu->process_times;
    const uint64_t process_tsc_total    = cpu->process_tsc_total;

    const uint64_t idle_tsc = process_tsc_total < cpu->hz ? cpu->hz - process_tsc_total : 0;
    const uint64_t avg_tsc_per_process = process_times ? process_tsc_total / process_times : 0;
    const uint64_t avg_n_pkts_per_process = process_times ? pkt_total / process_times : 0;

    if (likely(avg_tsc_per_process)) {
        worker->windows[worker->cur_window].busy = pkt_total;
        worker->windows[worker->cur_window].total = pkt_total
                + (idle_tsc / avg_tsc_per_process) * avg_n_pkts_per_process;
    }
    else {
        worker->windows[worker->cur_window].busy = 0;
        worker->windows[worker->cur_window].total = 0;
    }

    worker->cur_window++;
    if (worker->cur_window >= CPULOAD_MAX_WINDOWS) {
        worker->cur_window = 0;
    }

    cpu->pkt_total = 0;
    cpu->process_times = 0;
    cpu->process_tsc_total = 0;
}
