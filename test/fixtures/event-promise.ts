export interface EventPromiseOptions {
  errorEvent?: string;
}

export async function eventPromise<T = Event>(
  emitter: EventTarget,
  event: string,
  opts?: EventPromiseOptions,
): Promise<T> {
  return new Promise<T>((resolve, reject) => {
    const errorEvent = opts?.errorEvent ?? 'error';
    const onEvent = (evt: Event) => {
      emitter.removeEventListener(errorEvent, onError);
      resolve(evt as T);
    };
    const onError = (err: Event) => {
      emitter.removeEventListener(event, onEvent);
      reject(err);
    };
    emitter.addEventListener(event, onEvent, { once: true });
    emitter.addEventListener(errorEvent, onError, { once: true });
  });
}
