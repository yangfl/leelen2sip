struct LeelenSession {
  uint32_t id;
  struct in46_addr addr;
  char *ours;
  char *theirs;
};

void LeelenSession_destroy (struct LeelenSession *self) {
  free(self->ours);
  free(self->theirs);
}
