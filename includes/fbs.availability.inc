<?php

/**
 * @file
 * Handles availability information from the library system.
 */

/**
 * Implements provider availability, items.
 *
 * @param array $provider_ids
 *   Array of ting object ids (faust).
 *
 * @return array
 *   Availability info.
 */
function fbs_availability_items($provider_ids) {
  $avails = array();
  $service = fbs_service();
  if ($service) {
    try {
      $avails = $service->Catalog->getAvailability(
        $service->agencyId,
        $provider_ids
      );
    }
    catch (Exception $e) {
      watchdog_exception('fbs', $e);
    }
  }

  $result = array();
  foreach ($avails as $item) {
    $result[$item->recordId] = array(
      'available' => $item->available,
      'reservable' => $item->reservable,
    );
  }

  return $result;
}

/**
 * Implements provider availability, holdings.
 *
 * @param array $provider_ids
 *   Array of ting object ids (faust).
 *
 * @return array
 *   Holdings info.
 */
function fbs_availability_holdings($provider_ids) {
  $holdings = array();
  $service = fbs_service();
  if ($service) {
    try {
      $holdings = $service->Catalog->getHoldings(
        $service->agencyId,
        $provider_ids
      );
    }
    catch (Exception $e) {
      watchdog_exception($e);
    }
  }

  $result = array();
  $tmp_issues = array();
  foreach ($holdings as $item) {
    $result[$item->recordId] = array(
      'local_id' => $item->recordId,
      'reservable' => $item->reservable,
      'show_reservation_button' => $item->reservable,
      // Gets properly set in the loop later.
      'available' => FALSE,
      'holdings' => array(),
      'total_count' => 0,
      // We don't know about this one.
      'is_internet' => FALSE,
      // And FBS can't tell us this.
      'reserved_count' => 0,
      'is_periodical' => FALSE,
    );

    $placement_parts = array('branch', 'department', 'location', 'sublocation');
    foreach ($item->holdings as $item_holding) {
      $result_holding = array(
        'available_count' => 0,
        'total_count' => 0,
        // We have no idea about this either.
        'reference_count' => 0,
        'placement' => array(),
      );

      // Add in placement.
      foreach ($placement_parts as $part) {
        if (!empty($item_holding->{$part}->title)) {
          $result_holding['placement'][] = $item_holding->{$part}->title;
        }
      }
      $placement_string = implode(' > ', $result_holding['placement']);
      // Count available materials.
      foreach ($item_holding->materials as $material) {
        $result[$item->recordId]['total_count']++;
        $result_holding['total_count']++;
        if ($material->available) {
          $result_holding['available_count']++;
          $result[$item->recordId]['available'] = TRUE;
        }

        if ($material->periodical) {
          // Add to issues.
          $key = (!empty($material->periodical->volumeYear) ?
              $material->periodical->volumeYear : '') . ':' .
            $material->periodical->volume . ':' .
            (!empty($material->periodical->volumeNumber) ?
              $material->periodical->volumeNumber : '') . ':' .
            $placement_string;
          if (isset($tmp_issues[$key])) {
            $tmp_issues[$key]['available'] += $material->available ? 1 : 0;
            $tmp_issues[$key]['total'] += 1;
          }
          else {
            $tmp_issues[$key] = array(
              'vol' => $material->periodical->volume . (!empty($material->periodical->volumeNumber) ? '-' . $material->periodical->volumeNumber : ''),
              'year' => $material->periodical->volumeYear,
              'available' => $material->available ? 1 : 0,
              'total' => 1,
              'placement' => $placement_string,
              'local_id' => _fbs_periodical_get_local_id(
                $item->recordId,
                $material->periodical
              ),
            );
          }
        }
      }

      $result[$item->recordId]['holdings'][] = $result_holding;
    }

    // Add issues in the right nesting.
    if (!empty($tmp_issues)) {
      $issues = array();
      $result[$item->recordId]['is_periodical'] = TRUE;
      foreach ($tmp_issues as $issue) {
        if (!isset($issues[$issue['year']])) {
          $issues[$issue['year']] = array();
        }

        $year = &$issues[$issue['year']];

        if (!isset($year[$issue['vol']])) {
          $year[$issue['vol']] = array(
            'local_id' => $issue['local_id'],
            'placement' => array(),
          );
        }
        $year[$issue['vol']]['placement'][] = array(
          'reservable' => $item->reservable,
          'available_count' => $issue['available'],
          'location' => $issue['placement'],
          'total_count' => $issue['total'],
        );
      }
      $result[$item->recordId]['issues'] = $issues;
    }

    $result[$item->recordId]['html'] = _fbs_get_holdings(
      $result[$item->recordId],
      $result[$item->recordId]['is_periodical']
    );
  }

  return $result;

}

/**
 * Getting holdings for preprocessing.
 */
function _fbs_get_holdings($res, $is_periodical = FALSE) {
  if (isset($res)) {
    if ($is_periodical) {
      return _fbs_set_holdings_periodical($res);
    }
    else {
      return _fbs_set_holdings($res);
    }
  }
  else {
    return FALSE;
  }
}

/**
 * Set holdings for all kinds of material except periodicals.
 */
function _fbs_set_holdings($res) {
  $holdings = array();
  $reservation_count = 0;
  $reservation_count += (int) $res['reservation_count'];
  foreach ($res['holdings'] as $holding) {
    $holdings[] = $holding;
  }

  $result = _fbs_set_table_html($holdings, $reservation_count);
  return $result;
}

/**
 * Set holdings if material is periodical only.
 */
function _fbs_set_holdings_periodical($res) {
  $holdings = array();
  $reservation_count = 0;
  foreach ($res as $id => $records) {
    $reservation_count += (int) $records['reservation_count'];
    if (isset($records['holdings'])) {
      foreach ($records['holdings'] as $holding => $issue_year) {
        foreach ($issue_year as $key) {
          $holdings[] = $key[0];
        }
      }
    }
  }

  $result = _fbs_set_table_html($holdings, $reservation_count);
  return $result;
}

/**
 * Make the html-table.
 */
function _fbs_set_table_html($h, $r_count) {
  // Set a classname for styling the table.
  $variables['attributes'] = array(
    'class' => array(
      drupal_html_class('availability_holdings_table'),
    ),
  );

  // Set table header.
  $variables['header'] = array(
    'placement' => t('Placement'),
    'copies' => t('Copies'),
    'home' => t('At home'),
  );

  // Set table rows.
  $variables['rows'] = _fbs_set_rows($h);

  // Set last row with reservation count.
  $row = array();
  $row['data']['Library'] = t('Reservations');
  $row['data']['Copies'] = array('data' => $r_count, 'colspan' => 4);
  $variables['rows'][] = $row;

  return theme_table($variables);
}

/**
 * Set rows in table for given holdings.
 */
function _fbs_set_rows($h) {
  $rows = array();

  $copies_total = 0;
  $home_total = 0;

  foreach ($h as $key => $data) {
    $row = array();

    $row['placement'] = implode(' > ', $data['placement']);

    $row['copies'] = isset($data['total_count']) ? (int) $data['total_count'] : 0;
    $copies_total += $row['copies'];

    $row['home'] = isset($data['available_count']) ? (int) $data['available_count'] : 0;
    $home_total += $row['home'];

    $rows[] = $row;
  }

  if (count($rows) >= 1) {
    $rows = _clean_up_rows($rows);
  }
  // Adding row - totals.
  $row = array();
  $row['data']['Library'] = t('Total');
  $row['data']['Copies'] = $copies_total;
  $row['data']['Home'] = $home_total;
  $row['class'] = array(drupal_html_class('availability_holdings_last_row'));
  $rows[] = $row;

  return $rows;
}

/**
 * If the same placement exists several times collect them in one line.
 */
function _clean_up_rows($_rows) {
  $rows = array();
  $placements = array();

  foreach ($_rows as $row) {
    $currkey = $row['placement'];
    if (!in_array($currkey, $placements)) {
      $placements[] = $currkey;
      $placementsarr = _get_placements_with_key($_rows, $currkey);
      $this_row = _sum_placement($placementsarr);
      $rows[] = $this_row;
    }
  }
  return $rows;
}

/**
 * Collect materials with the same placement.
 */
function _get_placements_with_key($_rows, $currkey) {
  $rows = array();
  foreach ($_rows as $key) {
    if ($key['placement'] == $currkey) {
      $rows[] = $key;
    }
  }
  return $rows;
}

/**
 * Sum material for same placement in one row.
 */
function _sum_placement($placementsarr) {
  $row = $placementsarr[0];
  for ($i = 1; $i < count($placementsarr); $i++) {
    $next_row = $placementsarr[$i];
    $row['copies'] += $next_row['copies'];
    $row['home'] += $next_row['home'];
  }
  return $row;
}
