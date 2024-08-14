#pragma once
typedef struct list_head {
	struct list_head *next;
	struct list_head *prev;
	list_head() {
		next = prev = nullptr;
	}
	void list_init() {
		next = prev = nullptr;
	}

} list_head;