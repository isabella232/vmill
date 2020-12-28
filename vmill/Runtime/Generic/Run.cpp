/*
 * Copyright (c) 2017 Trail of Bits, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#if 1
# define STRACE_SYSCALL_NUM(nr) \
    do { \
      auto curr = __vmill_current(); \
      __vmill_strace( \
          ANSI_COLOR_YELLOW "%p %p %3" PRIuADDR ":" ANSI_COLOR_RESET, \
          reinterpret_cast<void *>(curr), \
          reinterpret_cast<void *>(curr->memory), \
          nr); \
    } while (false)

# define STRACE_ERROR(syscall, fmt, ...) \
    __vmill_strace(ANSI_COLOR_RED #syscall ":" fmt ANSI_COLOR_RESET "\n", \
                   ##__VA_ARGS__)

# define STRACE_SUCCESS(syscall, fmt, ...) \
    __vmill_strace(ANSI_COLOR_GREEN #syscall ":" fmt ANSI_COLOR_RESET "\n", \
                   ##__VA_ARGS__)
#else
# define STRACE_SYSCALL_NUM(...)
# define STRACE_ERROR(...)
# define STRACE_SUCCESS(...)
#endif

extern "C" uint64_t __vmill_initial_heap_end(const void *, vmill::PC, vmill::AddressSpace *);

// Initialize a task.
static void __vmill_init_task(
    vmill::Task *task, const void *state, vmill::PC pc,
    vmill::AddressSpace *memory) {

  task->state = new State;
  task->pc = pc;
  task->status = vmill::kTaskStatusRunnable;
  task->status_on_resume = vmill::kTaskStatusRunnable;
  task->location = vmill::kTaskNotYetStarted;
  task->memory = memory;
  task->async_routine = __vmill_allocate_coroutine();
  memcpy(task->state, state, sizeof(State));

  task->fpu_rounding_mode = __vmill_get_rounding_mode(task->state);
  task->program_break = __vmill_initial_heap_end(nullptr, pc, memory);
}

static void __vmill_fini_task(vmill::Task *task) {
  __vmill_free_coroutine(task->async_routine);
  task->async_routine = nullptr;
  delete task->state;
  task->state = nullptr;
}
