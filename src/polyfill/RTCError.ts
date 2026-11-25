export default class RTCError extends DOMException implements globalThis.RTCError {
  #errorDetail: globalThis.RTCErrorDetailType;
  #receivedAlert: number | null;
  #sctpCauseCode: number | null;
  #sdpLineNumber: number | null;
  #sentAlert: number | null;
  #httpRequestStatusCode: number | null;

  constructor(init: globalThis.RTCErrorInit, message?: string) {
    super(message, 'OperationError');

    if (!init?.errorDetail)
      throw new TypeError('Cannot construct RTCError, errorDetail is required');
    if (
      [
        'data-channel-failure',
        'dtls-failure',
        'fingerprint-failure',
        'hardware-encoder-error',
        'hardware-encoder-not-available',
        'sctp-failure',
        'sdp-syntax-error',
      ].indexOf(init.errorDetail) === -1
    )
      throw new TypeError('Cannot construct RTCError, errorDetail is invalid');

    this.#errorDetail = init.errorDetail;
    this.#receivedAlert = init.receivedAlert ?? null;
    this.#sctpCauseCode = init.sctpCauseCode ?? null;
    this.#sdpLineNumber = init.sdpLineNumber ?? null;
    this.#sentAlert = init.sentAlert ?? null;
    this.#httpRequestStatusCode = init.httpRequestStatusCode ?? null;
  }

  get errorDetail(): globalThis.RTCErrorDetailType {
    return this.#errorDetail;
  }

  get receivedAlert(): number | null {
    return this.#receivedAlert;
  }

  get sctpCauseCode(): number | null {
    return this.#sctpCauseCode;
  }

  get httpRequestStatusCode(): number | null {
    return this.#httpRequestStatusCode;
  }

  get sdpLineNumber(): number | null {
    return this.#sdpLineNumber;
  }

  get sentAlert(): number | null {
    return this.#sentAlert;
  }
}
