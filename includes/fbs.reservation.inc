<?php

/**
 * @file
 * Handles user reservations.
 */

/**
 * Implements provider reservation, list.
 *
 * @param object $account
 *   User to list reservations for.
 * @param string|null $requested_type
 *   Type of reservations to list. Null for all.
 *
 * @return array
 *   Reservation info.
 */
function fbs_reservation_list($account, $requested_type = NULL) {
  $res = array();
  $result = array();

  $service = fbs_service();
  if ($service) {
    try {
      $res = $service->Reservations->getReservations($service->agencyId, fbs_patron_id($account));
    }
    catch (Exception $e) {
      watchdog_exception('fbs', $e);
    }
  }
  $ids = array();
  foreach ($res as $reservation) {
    $ids[$reservation->reservationId] = $reservation->recordId;
  }
  $ids = ding_provider_build_entity_id($ids);

  foreach ($res as $reservation) {
    $properties = array(
      'id' => $reservation->reservationId,
      'ding_entity_id' => $ids[$reservation->recordId],
      'pickup_branch_id' => $reservation->pickupBranch,
      'created' => $reservation->dateOfReservation,
      'expiry' => $reservation->expiryDate,
      'queue_number' => $reservation->numberInQueue,
    );

    if (!empty($reservation->numberInQueue)) {
      $properties['ready_for_pickup'] = 0;
    }
    else {
      $properties += array(
        'pickup_order_id' => $reservation->pickupNumber,
        'pickup_date' => $reservation->pickupDeadline,
        'ready_for_pickup' => 1,
      );
    }
    $result[$reservation->reservationId] = new DingProviderReservation($reservation->reservationId, $properties);

    // Add in periodical information if it is one.
    if ($reservation->periodical) {
      $periodical = $reservation->periodical;
      $vol = $periodical->volume;
      if (!empty($periodical->volumeNumber)) {
        $vol .= '.' . $periodical->volumeNumber;
      }
      if (!empty($periodical->volumeYear)) {
        $ding_reservation['notes'] = t('Issue @vol, @year', array('@vol' => $vol, '@year' => $periodical->volumeYear));
      }
      else {
        $ding_reservation['notes'] = t('Issue @vol', array('@vol' => $vol));
      }

      $properties += $ding_reservation;
    }

    // Handle inter library loans.
    if (!empty($reservation->ilBibliographicRecord)) {
      $ding_reservation['title'] = $reservation->ilBibliographicRecord->title;

      $properties += $ding_reservation;
    }

    $result[$reservation->reservationId] = new DingProviderReservation($reservation->reservationId, $properties);
  }

  // Filter the result based on the requested type of reservations.
  if ($requested_type) {
    if (isset($result[$requested_type])) {
      return $result[$requested_type];
    }
    else {
      watchdog('fbs', 'Unknown reservation type (%type) requested', array('%type' => $requested_type), WATCHDOG_ERROR);
      return array();
    }
  }

  return $result;
}

/**
 * Implements provider reservation, create.
 *
 * @param object $account
 *   User to reserve for.
 * @param string $record_ids
 *   Id of ting object to reserve.
 * @param array $options
 *   Reservation options.
 *
 */
function fbs_reservation_create($account, $record_id, $options) {
  $batch = new FBS\Model\CreateReservationBatch();
  $batch->reservations = array();

  $reservation = new FBS\Model\CreateReservation();

  // We use our own custom ids prefixed with "fbs-" for periodicals.
  if (preg_match('/^fbs-/', $record_id)) {
    list($record_id, $periodical) = _fbs_periodical_parse_local_id($record_id);
    $reservation->recordId = $record_id;
    $reservation->periodical = $periodical;
  }
  else {
    $reservation->recordId = $record_id;
  }

  $reservation->expiryDate = date('Y-m-d', REQUEST_TIME + ($options['interest_period'] * 24 * 60 * 60));
  $reservation->pickupBranch = $options['preferred_branch'];
  $batch->reservations[] = $reservation;

  /** @var \FBS\Model\ReservationResponse $response */
  $service = fbs_service();
  if (!$service) {
    return FALSE;
  }
  try {
    $response = $service->Reservations->addReservations($service->agencyId, fbs_patron_id($account), $batch);
  }
  catch (Exception $e) {
    watchdog_exception('fbs', $e);
  }

  /** @var \FBS\Model\ReservationResult $result */
  $result = current($response->reservationResults);

  if ($response->success) {
    /** @var \FBS\Model\ReservationDetails $res_details */
    $res_details = $result->reservationDetails;
    return array(
      'branch' => $res_details->pickupBranch,
      'queue_number' => $res_details->numberInQueue,
    );
  }
  else {
    switch ($result->result) {
      case 'already_reserved':
        throw new DingProviderReservationExists();

      case 'patron_is_blocked':
      case 'loaning_profile_not_found':
      case 'patron_not_found':
        throw new DingProviderReservationNotAllowed();

      case 'material_not_loanable':
      case 'material_not_reservable':
      case 'material_lost':
      case 'material_not_found':
      case 'material_part_of_collection':
        throw new DingProviderReservationNotAvailable();

      // Success shouldn't happen.
      case 'success':
      default:
        watchdog('fbs', 'Unexpected state "@state" on failed reservaiton', array('@state' => $result->result), WATCHDOG_WARNING);
        throw new DingProviderUserException('Unknown error from library system while attempting to reserve.');
    }
  }

  return FALSE;
}

/**
 * Implements provider reservation, update.
 *
 * @param object $account
 *   User to update reservations for.
 * @param array $reservation_ids
 *   Reservations to update.
 * @param array $options
 *   Update options.
 */
function fbs_reservation_update($account, $reservation_ids, $options) {
  $batch = new FBS\Model\UpdateReservationBatch();
  $batch->reservations = array();
  foreach ($reservation_ids as $reservation_id) {
    $reservation = new FBS\Model\UpdateReservation();
    $reservation->reservationId = $reservation_id;
    $reservation->expiryDate = date('Y-m-d', REQUEST_TIME + ($options['interest_period'] * 24 * 60 * 60));
    $reservation->pickupBranch = $options['preferred_branch'];
    $batch->reservations[] = $reservation;
  }
  $service = fbs_service();
  if ($service) {
    $service->Reservations->updateReservations($service->agencyId, fbs_patron_id($account), $batch);
  }
}

/**
 * Implements provider reservation, delete.
 *
 * @param object $account
 *   User to delete reservations for.
 * @param array $reservation_ids
 *   Reservations to delete.
 */
function fbs_reservation_delete($account, $reservation_id) {
  $service = fbs_service();
  if ($service) {
    $service->Reservations->deleteReservations($service->agencyId, fbs_patron_id($account), $reservation_id);
  }
}

/**
 * Implements provider reservation, reservation_deletion_enabled.
 *
 * Check where reservation deletion have been enabled in the providers settings.
 *
 * @return bool
 *   Whether deletion is enabled.
 */
function fbs_reservation_reservation_deletion_enabled() {
  return variable_get('fbs_enable_reservation_deletion', FALSE);
}

/**
 * Implements provider reservation, branch_name.
 *
 * @param string $branch_id
 *   The branch id to look up.
 *
 * @return string|null
 *   The branch name or null if not found.
 */
function fbs_reservation_branch_name($branch_id) {
  $res = array();
  $service = fbs_service();
  if ($service) {
    $res = $service->Placement->getBranches($service->agencyId);
  }

  foreach ($res as $branch) {
    if ($branch->branchId == $branch_id) {
      return $branch->title;
    }
  }

  return NULL;
}

/**
 * Implements provider reservation, default_options.
 *
 * @param object $account
 *   The user to get default options for.
 *
 * @return array
 *   The default options.
 */
function fbs_reservation_default_options($account) {
  $patron_info = fbs_get_patron_info($account);
  return array(
    'fbs_preferred_branch' => $patron_info['preferred_branch'],
    'interest_period' => variable_get('fbs_default_interest_period', 180),
  );
}

/**
 * Implements provider reservation, options.
 *
 * @param object $account
 *   The user to get reservation options for.
 * @param string $default_branch
 *   Default branch to select.
 *
 * @return array
 *   The reservation options.
 */
function fbs_reservation_options($type, $account, $default_branch) {
  $patron_info = fbs_get_patron_info($account);

  $form = array();
  $form += ding_reservation_default_options_branch($type, 'preferred_branch', $patron_info['preferred_branch'], fbs_branches());
  $form += ding_reservation_interest_period_selector($type, 'interest_period', variable_get('fbs_default_interest_period', 180), fbs_interest_periods());

  return $form;
}

/**
 * Implements provider reservation, options_submit.
 *
 * @param object $account
 *   The user to get update preferred branch for.
 * @param array $values
 *   Form values.
 */
function fbs_reservation_options_submit($type, $account, $values, $reservation_info) {
  fbs_update_preferred_branch($account, $reservation_info['fbs_preferred_branch']);
}

/**
 * Implements provider reservation, default_interst_period.
 *
 * @return int
 *   Default interest period for user.
 */
function fbs_reservation_default_interest_period() {
  return variable_get('fbs_default_interest_period', 180);
}

/**
 * Get list of pickup branches.
 *
 * @return array
 *   FBS pickup branches.
 */
function fbs_reservation_pickup_branches() {
  $branches = fbs_branches();

  return $branches;
}

/**
 * Replace entity for the reservation in the case object in not in Ting anymore.
 *
 * @param $item
 *   Provider reservation object.
 *
 * @return \TingEntity
 *   Ting entity object, filled with data from provider service.
 */
function fbs_replace_entity($item) {
  $faust_number = explode(':', $item->ding_entity_id);
  $faust_number = $faust_number[1];

  // Build entity based on available information.
  $entity = new TingEntity();
  $entity->setLocalId($faust_number);
  $entity->setDingEntityId($item->ding_entity_id);
  $entity->setTitle($item->__get('title'));

  $entity->id = $item->ding_entity_id;
  $entity->ting_cover = array(LANGUAGE_NONE => array(array('id' => $item->ding_entity_id)));
  $entity->ting_title = array(LANGUAGE_NONE => array(array('id' => $item->ding_entity_id)));
  $entity->ding_entity_type = 'ding_entity';
  $entity->provider_id = $faust_number;

  return $entity;
}
