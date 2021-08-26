#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "utils/macro.h"
// #include "../number.h"
struct LeelenNumber;
#include "dialog.h"
#include "message.h"
#include "voip.h"


static void LeelenVoIP_shrink (struct LeelenVoIP *self) {
  int n_dialog = 0;
  for (int i = 0; i < self->n_dialog; i++) {
    struct LeelenDialog *dialog = self->dialogs[i];
    continue_if_fail (dialog != NULL);
    if likely (dialog->id != 0) {
      n_dialog = i;
    } else {
      LeelenDialog_destroy(dialog);
      free(dialog);
      self->dialogs[i] = NULL;
    }
  }
  n_dialog++;

  if unlikely (self->n_dialog != n_dialog) {
    struct LeelenDialog **dialogs = realloc(
      self->dialogs, n_dialog * sizeof(struct LeelenDialog *));
    if likely (dialogs != NULL || n_dialog == 0) {
      self->n_dialog = n_dialog;
      self->dialogs = dialogs;
    }
  }
}


int LeelenVoIP_receive (
    struct LeelenVoIP *self, char *msg, int sockfd,
    char ***audio_formats, char ***video_formats, const struct sockaddr *src) {
  struct LeelenDialog *dialog;

  pthread_rwlock_rdlock(&self->lock);
  for (int i = 0; i < self->n_dialog; i++) {
    if unlikely (self->dialogs[i]->id == LEELEN_MESSAGE_ID(msg)) {
      dialog = self->dialogs[i];
      pthread_rwlock_unlock(&self->lock);
      goto end;
    }
  }
  pthread_rwlock_unlock(&self->lock);

  dialog = LeelenVoIP_connect(self, src, NULL, LEELEN_MESSAGE_ID(msg));
  return_if_fail (dialog != NULL) -1;

end:
  return LeelenDialog_receive(
    dialog, msg, sockfd, audio_formats, video_formats);
}


struct LeelenDialog *LeelenVoIP_connect (
    struct LeelenVoIP *self, const struct sockaddr *dst,
    const struct LeelenNumber *to, leelen_id_t id) {
  pthread_rwlock_wrlock(&self->lock);

  LeelenVoIP_shrink(self);

  int i;
  for (i = 0; i < self->n_dialog; i++) {
    goto_if_fail (self->dialogs[i] != NULL) init;
  }
  {
    struct LeelenDialog **dialogs = realloc(
      self->dialogs, (self->n_dialog + 1) * sizeof(struct LeelenDialog *));
    should (dialogs != NULL) otherwise {
      pthread_rwlock_unlock(&self->lock);
      return NULL;
    }
    self->n_dialog++;
    self->dialogs = dialogs;
  }

init:
  self->dialogs[i] = malloc(sizeof(struct LeelenDialog));
  struct LeelenDialog *dialog = self->dialogs[i];
  should (dialog != NULL) otherwise {
    pthread_rwlock_unlock(&self->lock);
    return NULL;
  }
  LeelenDialog_init(dialog, self->config, dst, to, id);

  pthread_rwlock_unlock(&self->lock);
  return dialog;
}


void LeelenVoIP_destroy (struct LeelenVoIP *self) {
  pthread_rwlock_destroy(&self->lock);
  return_if_fail (self->dialogs != NULL);
  for (int i = 0; i < self->n_dialog; i++) {
    continue_if (self->dialogs[i] == NULL);
    LeelenDialog_destroy(self->dialogs[i]);
    free(self->dialogs[i]);
  }
  free(self->dialogs);
}
